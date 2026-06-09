var Clay = require('pebble-clay');

var config = [
  {
    type: 'heading',
    defaultValue: 'UNO Watchface'
  },
  {
    type: 'select',
    messageKey: 'SHAKE_ACTION',
    label: 'Al sacudir la muñeca',
    defaultValue: '0',
    options: [
      { label: 'Nada',    value: '0' },
      { label: 'Modo 18', value: '1' },
      { label: 'Sonido',  value: '2' }
    ]
  },
  {
    type: 'select',
    messageKey: 'SHOW_SECONDS',
    label: 'Números secundarios',
    defaultValue: '0',
    options: [
      { label: 'Mostrar fecha actual', value: '0' },
      { label: 'Mostrar segundos',     value: '1' }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Guardar'
  }
];

// autoHandleEvents:false para manejar el envio con keys enteros
// (evita el bug de message_keys que devuelve void 0 en el bundle)
var clay = new Clay(config, null, { autoHandleEvents: false });

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;

  // getSettings(response, false) retorna {SHAKE_ACTION: {value: "0"|"1"|"2"}, ...}
  // el valor está envuelto en {value:...}, hay que acceder a .value
  var settings = clay.getSettings(e.response, false);
  console.log('UNO config raw: ' + JSON.stringify(settings));

  var msg = {};
  if (settings.SHAKE_ACTION !== undefined) {
    msg[4] = parseInt(settings.SHAKE_ACTION.value, 10) || 0;
  }
  if (settings.SHOW_SECONDS !== undefined) {
    msg[5] = parseInt(settings.SHOW_SECONDS.value, 10) || 0;
  }

  console.log('UNO config enviando: ' + JSON.stringify(msg));

  Pebble.sendAppMessage(msg,
    function()    { console.log('UNO config: OK'); },
    function(err) { console.log('UNO config: error: ' + JSON.stringify(err)); }
  );
});
