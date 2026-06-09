var Clay = require('pebble-clay');

var config = [
  {
    type: 'heading',
    defaultValue: 'UNO Watchface'
  },
  {
    type: 'toggle',
    messageKey: 'SHOW_DIECIOCHO',
    label: 'Modo 18',
    description: 'Activar con el movimiento de la muñeca',
    defaultValue: false
  },
  {
    type: 'toggle',
    messageKey: 'SHOW_SECONDS',
    label: 'Mostrar segundos',
    description: 'Reemplaza la fecha con los segundos actuales',
    defaultValue: false
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

  // getSettings(response, false) retorna {SHOW_DIECIOCHO: {value: true/false}}
  // el valor está envuelto en {value:...}, hay que acceder a .value
  var settings = clay.getSettings(e.response, false);
  console.log('UNO config raw: ' + JSON.stringify(settings));

  var msg = {};
  if (settings.SHOW_DIECIOCHO !== undefined) {
    msg[4] = settings.SHOW_DIECIOCHO.value ? 1 : 0;
  }
  if (settings.SHOW_SECONDS !== undefined) {
    msg[5] = settings.SHOW_SECONDS.value ? 1 : 0;
  }

  console.log('UNO config enviando: ' + JSON.stringify(msg));

  Pebble.sendAppMessage(msg,
    function()    { console.log('UNO config: OK'); },
    function(err) { console.log('UNO config: error: ' + JSON.stringify(err)); }
  );
});
