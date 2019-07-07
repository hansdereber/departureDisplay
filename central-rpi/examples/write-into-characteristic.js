var noble = require('noble');

var uartServiceUuid = '6E400001-B5A3-F393-E0A9-E50E24DCCA9E';
var uartCharacteristicRxUuid = '6E400002-B5A3-F393-E0A9-E50E24DCCA9E';
var uartCharacteristicRxUuid = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E';
var peripheralAddress = 'e9:fa:f5:f5:4f:ba';

var uartClientService = null;
var displayTextCharacteristic = null;

noble.on('stateChange', function(state) {
    if (state === 'poweredOn') {
      //
      // Once the BLE radio has been powered on, it is possible
      // to begin scanning for services. Pass an empty array to
      // scan for all services (uses more time and power).
      //
      console.log('scanning...');
      
      noble.startScanning();
    }
    else {
      noble.stopScanning();
    }
});

noble.on('discover', function(peripheral) {
    if (peripheral.address === peripheralAddress) {
        noble.stopScanning();

        console.log('Discovered device with name ' + peripheral.advertisement.localName + ' with id ' + peripheral.id +
                ' with address <' + peripheral.address +  '>');
        
    peripheral.connect();
    }
});

