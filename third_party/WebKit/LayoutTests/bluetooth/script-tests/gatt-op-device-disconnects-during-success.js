'use strict';
promise_test(() => {
  let val = new Uint8Array([1]);
  return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['health_thermometer']}],
      optionalServices: [request_disconnection_service_uuid]
    }))
    .then(device => device.gatt.connect())
    .then(gattServer => {
      let measurement_interval;
      return gattServer
        .getPrimaryService('health_thermometer')
        .then(ht=> ht.getCharacteristic('measurement_interval'))
        .then(mi => measurement_interval = mi)
        .then(() => gattServer.getPrimaryService(
          request_disconnection_service_uuid))
        .then(service => service.getCharacteristic(
          request_disconnection_characteristic_uuid))
        .then(requestDisconnection => {
          requestDisconnection.writeValue(new Uint8Array([0]));
          return assert_promise_rejects_with_message(
            measurement_interval.CALLS([readValue()]),
            new DOMException(
              'GATT Server disconnected while performing a GATT operation.',
              'NetworkError'));
        });
    });
}, 'Device disconnects during a FUNCTION_NAME call that succeeds. ' +
   'Reject with NetworkError.');
