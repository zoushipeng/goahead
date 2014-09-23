Expansive.load({
    meta: {
        title:       'Embedthis GoAhead Documentation',
        url:         'https://embedthis.com/goahead/doc/',
        description: 'Simple, fast, secure embedded web server',
        keywords:    'goahead, embedded web server, embedded, devices, http server, internet of things, appweb',
    },
    expansive: {
        copy:    [ 'images' ],
        dependencies: { 'css/all.css.less': 'css/*.inc.less' },
        documents: [ '**', '!css/*.inc.less' ],
        plugins: [ 'less' ],
    }
})
