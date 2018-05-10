// ----------------------------------------------------------------------------
// ModularDeviceBase.cpp
//
//
// Authors:
// Peter Polidoro polidorop@janelia.hhmi.org
// ----------------------------------------------------------------------------
#include "../ModularDeviceBase.h"


using namespace modular_device_base;

ModularDeviceBase::ModularDeviceBase()
{
  // Enable watchdog
  watchdog_.reset();
  watchdog_reset_time_ = millis();
  watchdog_.enable(constants::watchdog_timeout);
}

void ModularDeviceBase::setup()
{
  // Server Setup
  modular_server_.setup();

  // Reset Watchdog
  resetWatchdog();

  // Variable Setup
  system_reset_ = false;

  // Add Server Streams
  modular_server_.addServerStream(Serial);
  for (size_t i=0; i<constants::SERIAL_STREAM_COUNT; ++i)
  {
    modular_server_.addServerStream(*(constants::serial_stream_ptrs[i]));
  }

  // Add Client Streams
  for (size_t i=0; i<constants::SERIAL_STREAM_COUNT; ++i)
  {
    client_streams_[i].setStream(*(constants::serial_stream_ptrs[i]));
    client_streams_[i].setId(constants::client_stream_ids[i]);
    client_streams_[i].setName(*(constants::client_stream_name_ptrs[i]));
  }

  // Set Device ID
  modular_server_.setDeviceName(constants::device_name);
  modular_server_.setFormFactor(constants::form_factor);

  // Add Hardware
  modular_server_.addHardware(constants::processor_hardware_info,
                              processor_pins_);

#if !defined(__AVR_ATmega2560__)
  modular_server_.addHardware(constants::hardware_info,
                              pins_);
#endif

  // Pins
#if !defined(__AVR_ATmega2560__)
  modular_server_.createPin(constants::bnc_a_pin_name,
                            constants::bnc_a_pin_number);

  modular_server_.createPin(constants::bnc_b_pin_name,
                            constants::bnc_b_pin_number);

  modular_server_.createPin(constants::btn_a_pin_name,
                            constants::btn_a_pin_number);

  modular_server::Pin & led_green_pin = modular_server_.createPin(constants::led_green_pin_name,
                                                                  constants::led_green_pin_number);
  led_green_pin.setModeDigitalOutput();
  led_green_pin.setValue(HIGH);

  modular_server::Pin & led_yellow_pin = modular_server_.createPin(constants::led_yellow_pin_name,
                                                                   constants::led_yellow_pin_number);
  led_yellow_pin.setModeDigitalOutput();

#endif

#if defined(__MK64FX512__)
  modular_server_.createPin(constants::btn_b_pin_name,
                            constants::btn_b_pin_number);


#endif

  // Add Firmware
  modular_server_.addFirmware(constants::firmware_info,
                              properties_,
                              parameters_,
                              functions_,
                              callbacks_);

  // Properties
  modular_server::Property & time_zone_offset_property = modular_server_.createProperty(constants::time_zone_offset_property_name,constants::time_zone_offset_default);
  time_zone_offset_property.setRange(constants::time_zone_offset_min,constants::time_zone_offset_max);

  // Parameters
  modular_server::Parameter & address_parameter = modular_server_.createParameter(constants::address_parameter_name);
  address_parameter.setRange(constants::address_min,constants::address_max);
  address_parameter.setArrayLengthRange(constants::address_array_length_min,constants::address_array_length_max);
  address_parameter.setTypeLong();

  modular_server::Parameter & request_parameter = modular_server_.createParameter(constants::request_parameter_name);
  request_parameter.setArrayLengthRange(constants::request_array_length_min,constants::request_array_length_max);
  request_parameter.setTypeAny();

  modular_server::Parameter & epoch_time_parameter = modular_server_.createParameter(constants::epoch_time_parameter_name);
  epoch_time_parameter.setRange(constants::epoch_time_min,constants::epoch_time_max);
  epoch_time_parameter.setUnits(constants::seconds_units);

  modular_server::Parameter & adjust_time_parameter = modular_server_.createParameter(constants::adjust_time_parameter_name);
  adjust_time_parameter.setTypeLong();
  adjust_time_parameter.setUnits(constants::seconds_units);

  // Functions
  modular_server::Function & forward_to_address_function = modular_server_.createFunction(constants::forward_to_address_function_name);
  forward_to_address_function.attachFunctor(makeFunctor((Functor0 *)0,*this,&ModularDeviceBase::forwardToAddressHandler));
  forward_to_address_function.addParameter(address_parameter);
  forward_to_address_function.addParameter(request_parameter);
  forward_to_address_function.setResultTypeObject();

  modular_server::Function & get_client_info_function = modular_server_.createFunction(constants::get_client_info_function_name);
  get_client_info_function.attachFunctor(makeFunctor((Functor0 *)0,*this,&ModularDeviceBase::getClientInfoHandler));
  get_client_info_function.setResultTypeObject();

  modular_server::Function & set_time_function = modular_server_.createFunction(constants::set_time_function_name);
  set_time_function.attachFunctor(makeFunctor((Functor0 *)0,*this,&ModularDeviceBase::setTimeHandler));
  set_time_function.addParameter(epoch_time_parameter);

  modular_server::Function & get_time_function = modular_server_.createFunction(constants::get_time_function_name);
  get_time_function.attachFunctor(makeFunctor((Functor0 *)0,*this,&ModularDeviceBase::getTimeHandler));
  get_time_function.setResultTypeLong();

  modular_server::Function & adjust_time_function = modular_server_.createFunction(constants::adjust_time_function_name);
  adjust_time_function.attachFunctor(makeFunctor((Functor0 *)0,*this,&ModularDeviceBase::adjustTimeHandler));
  adjust_time_function.addParameter(adjust_time_parameter);

  modular_server::Function & now_function = modular_server_.createFunction(constants::now_function_name);
  now_function.attachFunctor(makeFunctor((Functor0 *)0,*this,&ModularDeviceBase::nowHandler));
  now_function.setResultTypeObject();

  // Callbacks
  modular_server::Callback & reset_callback = modular_server_.createCallback(constants::reset_callback_name);
  reset_callback.attachFunctor(makeFunctor((Functor1<modular_server::Pin *> *)0,*this,&ModularDeviceBase::resetHandler));

  // Begin Streams
  Serial.begin(constants::baud);
  Serial.setTimeout(constants::serial_timeout);
  for (size_t i=0; i<constants::SERIAL_STREAM_COUNT; ++i)
  {
    constants::serial_stream_ptrs[i]->setTimeout(constants::serial_timeout);
    constants::serial_stream_ptrs[i]->begin(constants::baud);
  }

}

void ModularDeviceBase::startServer()
{
  // Start Modular Device Server
  modular_server_.startServer();
}

void ModularDeviceBase::update()
{
  if (!system_reset_ && ((millis() - watchdog_reset_time_) >= constants::watchdog_reset_duration))
  {
    resetWatchdog();
  }

  modular_server_.handleServerRequests();
}

void ModularDeviceBase::reset()
{
  system_reset_ = true;
}

void ModularDeviceBase::setTime(const time_t epoch_time)
{
  ::setTime(epoch_time);
}

time_t ModularDeviceBase::getTime()
{
  return ::now();
}

void ModularDeviceBase::adjustTime(const long adjust_time)
{
  ::adjustTime(adjust_time);
}

time_t ModularDeviceBase::now()
{
  return ::now();
}

void ModularDeviceBase::resetWatchdog()
{
  watchdog_reset_time_ = millis();
  watchdog_.reset();
}

bool ModularDeviceBase::timeIsSet()
{
  timeStatus_t time_status = timeStatus();
  return (time_status != timeNotSet);
}

time_t ModularDeviceBase::epochTimeToLocalTime(const time_t epoch_time)
{
  long time_zone_offset;
  modular_server_.property(constants::time_zone_offset_property_name).getValue(time_zone_offset);

  return epoch_time + time_zone_offset*constants::seconds_per_hour;
}

void ModularDeviceBase::writeDateTimeToResponse(const time_t time)
{
  time_t local_time = epochTimeToLocalTime(time);

  modular_server_.response().beginObject();

  modular_server_.response().write(constants::year_string,year(local_time));
  modular_server_.response().write(constants::month_string,month(local_time));
  modular_server_.response().write(constants::day_string,day(local_time));
  modular_server_.response().write(constants::hour_string,hour(local_time));
  modular_server_.response().write(constants::minute_string,minute(local_time));
  modular_server_.response().write(constants::second_string,second(local_time));

  modular_server_.response().endObject();
}

JsonStream * ModularDeviceBase::findClientJsonStream(const size_t stream_id)
{
  JsonStream * json_stream_ptr = NULL;
  int stream_index = findClientStreamIndex(stream_id);
  if (stream_index >= 0)
  {
    json_stream_ptr = &(client_streams_[stream_index].getJsonStream());
  }
  return json_stream_ptr;
}

int ModularDeviceBase::findClientStreamIndex(const size_t stream_id)
{
  int stream_index = -1;
  for (size_t i=0; i<constants::CLIENT_STREAM_COUNT; ++i)
  {
    if (stream_id == client_streams_[i])
    {
      stream_index = i;
      break;
    }
  }
  return stream_index;
}

int ModularDeviceBase::findClientStreamIndex(Stream & Stream)
{
  int stream_index = -1;
  for (size_t i=0; i<constants::CLIENT_STREAM_COUNT; ++i)
  {
    if (&Stream == &(client_streams_[i].getStream()))
    {
      stream_index = i;
      break;
    }
  }
  return stream_index;
}

// Handlers must be non-blocking (avoid 'delay')
//
// modular_server_.parameter(parameter_name).getValue(value) value type must be either:
// fixed-point number (int, long, etc.)
// floating-point number (float, double)
// bool
// const char *
// ArduinoJson::JsonArray *
// ArduinoJson::JsonObject *
// const ConstantString *
//
// For more info read about ArduinoJson parsing https://github.com/janelia-arduino/ArduinoJson
//
// modular_server_.property(property_name).getValue(value) value type must match the property default type
// modular_server_.property(property_name).setValue(value) value type must match the property default type
// modular_server_.property(property_name).getElementValue(element_index,value) value type must match the property array element default type
// modular_server_.property(property_name).setElementValue(element_index,value) value type must match the property array element default type

void ModularDeviceBase::forwardToAddressHandler()
{
  ArduinoJson::JsonArray * address_array_ptr;
  modular_server_.parameter(constants::address_parameter_name).getValue(address_array_ptr);

  ArduinoJson::JsonArray * request_array_ptr;
  modular_server_.parameter(constants::request_parameter_name).getValue(request_array_ptr);

  forwardToAddress(*address_array_ptr,*request_array_ptr);

}

void ModularDeviceBase::getClientInfoHandler()
{
  modular_server_.response().writeResultKey();

  modular_server_.response().beginArray();

  for (size_t client_index=0; client_index<clients_.size(); ++client_index)
  {
    ModularClient & client = clients_[client_index];

    modular_server_.response().beginObject();

    int client_stream_index = findClientStreamIndex(client.getStream());
    if (client_stream_index >= 0)
    {
      const ConstantString & stream_name = client_streams_[client_stream_index].getName();
      modular_server_.response().write(constants::stream_string,stream_name);
    }

    modular_server_.response().write(constants::address_parameter_name,client.getAddress());

    modular_server_.response().endObject();
  }

  modular_server_.response().endArray();
}

void ModularDeviceBase::resetHandler(modular_server::Pin * pin_ptr)
{
  reset();
}

void ModularDeviceBase::setTimeHandler()
{
  long epoch_time;
  modular_server_.parameter(constants::epoch_time_parameter_name).getValue(epoch_time);
  ModularDeviceBase::setTime(epoch_time);
}

void ModularDeviceBase::getTimeHandler()
{
  if (!timeIsSet())
  {
    modular_server_.response().returnError(constants::time_not_set_error);
    return;
  }
  time_t epoch_time = getTime();
  modular_server_.response().returnResult(epoch_time);
}

void ModularDeviceBase::adjustTimeHandler()
{
  long adjust_time;
  modular_server_.parameter(constants::adjust_time_parameter_name).getValue(adjust_time);
  ModularDeviceBase::adjustTime(adjust_time);
}

void ModularDeviceBase::nowHandler()
{
  if (!timeIsSet())
  {
    modular_server_.response().returnError(constants::time_not_set_error);
    return;
  }
  time_t time_now = ModularDeviceBase::now();
  modular_server_.response().writeResultKey();
  writeDateTimeToResponse(time_now);
}
