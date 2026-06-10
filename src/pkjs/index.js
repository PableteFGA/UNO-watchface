var Clay = require('pebble-clay');

var STRINGS = {
  es: {
    heading:        'UNO Watchface',
    welcome_label:  'Al volver al reloj',
    shake_label:    'Al sacudir la muñeca',
    secondary_label:'Números secundarios',
    nothing:        'Nada',
    mode18:         'Modo 18',
    sound:          'Sonido',
    show_date:      'Mostrar fecha actual',
    show_seconds:   'Mostrar segundos',
    save:           'Guardar'
  },
  en: {
    heading:        'UNO Watchface',
    welcome_label:  'When returning to watch',
    shake_label:    'On wrist shake',
    secondary_label:'Secondary display',
    nothing:        'Nothing',
    mode18:         'Mode 18',
    sound:          'Sound',
    show_date:      'Show current date',
    show_seconds:   'Show seconds',
    save:           'Save'
  },
  pt: {
    heading:        'UNO Watchface',
    welcome_label:  'Ao voltar ao relógio',
    shake_label:    'Ao agitar o pulso',
    secondary_label:'Mostrador secundário',
    nothing:        'Nada',
    mode18:         'Modo 18',
    sound:          'Som',
    show_date:      'Mostrar data atual',
    show_seconds:   'Mostrar segundos',
    save:           'Salvar'
  },
  it: {
    heading:        'UNO Watchface',
    welcome_label:  'Al ritorno al orologio',
    shake_label:    'Agitando il polso',
    secondary_label:'Display secondario',
    nothing:        'Niente',
    mode18:         'Modo 18',
    sound:          'Suono',
    show_date:      'Mostra data attuale',
    show_seconds:   'Mostra secondi',
    save:           'Salva'
  },
  de: {
    heading:        'UNO Watchface',
    welcome_label:  'Bei Rückkehr zur Uhr',
    shake_label:    'Bei Schütteln des Handgelenks',
    secondary_label:'Sekundäranzeige',
    nothing:        'Nichts',
    mode18:         'Modus 18',
    sound:          'Ton',
    show_date:      'Datum anzeigen',
    show_seconds:   'Sekunden anzeigen',
    save:           'Speichern'
  },
  fr: {
    heading:        'UNO Watchface',
    welcome_label:  'Au retour à la montre',
    shake_label:    'En secouant le poignet',
    secondary_label:'Affichage secondaire',
    nothing:        'Rien',
    mode18:         'Mode 18',
    sound:          'Son',
    show_date:      'Afficher la date',
    show_seconds:   'Afficher les secondes',
    save:           'Enregistrer'
  },
  ca: {
    heading:        'UNO Watchface',
    welcome_label:  'En tornar al rellotge',
    shake_label:    'En sacsejar el canell',
    secondary_label:'Pantalla secundària',
    nothing:        'Res',
    mode18:         'Mode 18',
    sound:          'So',
    show_date:      'Mostrar data actual',
    show_seconds:   'Mostrar segons',
    save:           'Desar'
  }
};

function getStrings(lang) {
  var code = (lang || 'es').substring(0, 2).toLowerCase();
  return STRINGS[code] || STRINGS.es;
}

function buildConfig(s) {
  return [
    { type: 'heading', defaultValue: s.heading },
    {
      type: 'select', messageKey: 'WELCOME_ACTION',
      label: s.welcome_label, defaultValue: '0',
      options: [
        { label: s.nothing, value: '0' },
        { label: s.mode18,  value: '1' },
        { label: s.sound,   value: '2' }
      ]
    },
    {
      type: 'select', messageKey: 'SHAKE_ACTION',
      label: s.shake_label, defaultValue: '0',
      options: [
        { label: s.nothing, value: '0' },
        { label: s.mode18,  value: '1' },
        { label: s.sound,   value: '2' }
      ]
    },
    {
      type: 'select', messageKey: 'SHOW_SECONDS',
      label: s.secondary_label, defaultValue: '0',
      options: [
        { label: s.show_date,    value: '0' },
        { label: s.show_seconds, value: '1' }
      ]
    },
    { type: 'submit', defaultValue: s.save }
  ];
}

var clay = null;

function initClay(lang) {
  var s = getStrings(lang);
  clay = new Clay(buildConfig(s), null, { autoHandleEvents: false });
}

Pebble.addEventListener('ready', function() {
  var lang = 'es';
  try { lang = Pebble.getActiveWatchInfo().language || 'es'; } catch(e) {}
  initClay(lang);
});

Pebble.addEventListener('showConfiguration', function() {
  if (!clay) initClay('es');
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;

  var settings = clay.getSettings(e.response, false);
  console.log('UNO config raw: ' + JSON.stringify(settings));

  var msg = {};
  if (settings.WELCOME_ACTION !== undefined) {
    msg[0] = parseInt(settings.WELCOME_ACTION.value, 10) || 0;
  }
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
