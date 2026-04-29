var Clay = require('pebble-clay');

function buildConfig() {
  var platform = 'basalt';
  try { platform = Pebble.getActiveWatchInfo().platform; } catch(e) {}

  var isEmery  = platform === 'emery';
  var isColor  = platform === 'emery' || platform === 'basalt';

  var items = [
    {
      type: 'heading',
      defaultValue: 'UNO Watchface'
    },
    {
      type: 'toggle',
      messageKey: 'SHOW_WELCOME',
      label: 'Mensaje de bienvenida',
      description: 'Mostrar "DANDO LA HORA – HECHO EN CHILE" al iniciar',
      defaultValue: true
    },
    {
      type: 'section',
      items: buildDialSection(isEmery, isColor)
    },
    {
      type: 'submit',
      defaultValue: 'Guardar'
    }
  ];

  return items;
}

function buildDialSection(isEmery, isColor) {
  var items = [
    {
      type: 'heading',
      defaultValue: 'Diseño del Dial',
      size: 2
    }
  ];

  if (isEmery) {
    items.push({
      type: 'toggle',
      messageKey: 'TRANSPARENT_PORTION',
      label: 'Porción transparente',
      description: 'Usar fondo transparente del reloj original',
      defaultValue: true
    });
  }

  if (isColor) {
    var colorItem = {
      type: 'color',
      messageKey: 'BG_COLOR',
      label: 'Color de fondo',
      defaultValue: 'DarkGray',
      sunlight: true
    };
    if (isEmery) colorItem.showIf = '!TRANSPARENT_PORTION';
    items.push(colorItem);
  }

  var shapeItem = {
    type: 'select',
    messageKey: 'DIAL_SHAPE',
    label: 'Diseño de pantalla',
    defaultValue: '0',
    options: [
      { label: 'Hexagonal', value: '0' },
      { label: 'Rectangular', value: '1' }
    ]
  };
  if (isEmery) shapeItem.showIf = '!TRANSPARENT_PORTION';
  items.push(shapeItem);

  return items;
}

// autoHandleEvents:false — manejamos showConfiguration y webviewclosed manualmente
// para evitar que Clay use message_keys (que devuelve void 0 en el bundle)
var clay = new Clay(buildConfig(), null, { autoHandleEvents: false });

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;

  var raw = e.response;
  // Clay devuelve JSON URL-encoded; decodificar si no empieza con '{'
  if (raw.charAt(0) !== '{') {
    try { raw = decodeURIComponent(raw); } catch(ex) {}
  }

  var settings;
  try { settings = JSON.parse(raw); } catch(ex) {
    console.log('UNO config: JSON parse error: ' + ex);
    return;
  }

  // Mapeo directo a claves enteras (coinciden con MESSAGE_KEY_* en C)
  var msg = {};
  if (settings.SHOW_WELCOME        !== undefined) msg[0] = Number(settings.SHOW_WELCOME);
  if (settings.TRANSPARENT_PORTION !== undefined) msg[1] = Number(settings.TRANSPARENT_PORTION);
  if (settings.BG_COLOR            !== undefined) msg[2] = Number(settings.BG_COLOR);
  if (settings.DIAL_SHAPE          !== undefined) msg[3] = parseInt(settings.DIAL_SHAPE, 10);

  console.log('UNO config sending: ' + JSON.stringify(msg));

  Pebble.sendAppMessage(msg,
    function()  { console.log('UNO config: sent OK'); },
    function(err) { console.log('UNO config: send error: ' + JSON.stringify(err)); }
  );
});
