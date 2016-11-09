'use strict';
promise_test(() => {
  let val = new Uint8Array([1]);
  return setBluetoothFakeAdapter('FailingGATTOperationsAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: [errorUUID(0xA0)]}],
      optionalServices: [request_disconnection_service_uuid]
    }))
    .then(device => device.gatt.connect())
    .then(gattServer => {
      let error_characteristic;
      return gattServer
        .getPrimaryService(errorUUID(0xA0))
        .then(es => es.getCharacteristic(errorUUID(0xA1)))
        .then(ec => error_characteristic = ec)
        .then(() => gattServer.getPrimaryService(
          request_disconnection_service_uuid))
        .then(service => service.getCharacteristic(
          request_disconnection_characteristic_uuid))
        .then(requestDisconnection => {
          requestDisconnection.writeValue(new Uint8Array([0]));
          return assert_promise_rejects_with_message(
            error_characteristic.CALLS([readValue()]),
            new DOMException(
              'GATT Server disconnected while performing a GATT operation.',
              'NetworkError'));
        });
    });
}, 'Device disconnects during a FUNCTION_NAME call that fails. ' +
   'Reject with NetworkError.');
