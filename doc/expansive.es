Expansive.load({
    meta: {
        title:       'Embedthis GoAhead Documentation',
        url:         'https://embedthis.com/goahead/doc/',
        _description: 'Simple, fast, secure embedded web server',
    },
    expansive: {
        copy:    [ 'images' ],
        dependencies: { 'css/all.css.less': '**.less' },
        documents: [ '**', '!**.less', '**.css.less' ],
        plugins: [ 'less' ],
    }
})
