/*
    Support functions for Embedthis GoAhead

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

require ejs.tar
require ejs.unix

public function packageBinaryFiles(formats = ['tar', 'native'], minimal = false) {
    let settings = bit.settings
    let bin = bit.dir.pkg.join('bin')
    safeRemove(bit.dir.pkg)
    let vname = settings.product + '-' + settings.version + '-' + settings.buildNumber
    let pkg = bin.join(vname)
    pkg.makeDir()

    let contents = pkg.join('contents')

    let prefixes = bit.prefixes;
    let p = {}
    for (prefix in bit.prefixes) {
        p[prefix] = Path(contents.portable.name + bit.prefixes[prefix].removeDrive().portable)
        p[prefix].makeDir()
    }
    let strip = bit.platform.profile == 'debug'

    if (!bit.cross) {
        /* These three files are replicated outside the data directory */
        if (!minimal) {
            install('doc/product/README.TXT', pkg, {fold: true, expand: true})
            install('package/install.sh', pkg.join('install'), {permissions: 0755, expand: true})
            install('package/uninstall.sh', pkg.join('uninstall'), {permissions: 0755, expand: true})
            if (bit.platform.os == 'windows') {
                install('package/windows/LICENSE.TXT', bin, {fold: true, expand: true})
            }
            install(['doc/licenses/*.txt'], p.product.join('LICENSE.TXT'), {
                cat: true,
                textfile: true,
                fold: true,
                title: bit.settings.title + ' Licenses',
            })
            install('doc/product/README.TXT', p.product, {fold: true, expand: true})
            install('package/uninstall.sh', p.bin.join('uninstall'), {permissions: 0755, expand: true})
            install('package/linkup', p.bin, {permissions: 0755})
        }
        install('src/web', p.web, {exclude: /mgmt|bench/, subtree: true})
        if (!minimal) {
            /* KEEP
            install('src/server/web/test/*', p.web.join('test'), {
                include: /.cgi|test.pl|test.py/,
                permissions: 0755,
            })
            */
        }
        install('src/auth.txt', p.config)
        install('src/route.txt', p.config)
        //KEEP let user = getWebUser(), group = getWebUser()
    }
    install(bit.dir.bin + '/*', p.bin, {
        include: /goahead|webcom|libgo|\.dll/,
        exclude: /\.pdb|\.exp|\.lib|\.def|\.suo|\.old/,
        permissions: 0755, 
    })
    if (bit.platform.os != 'windows') {
        install(bit.dir.bin.join('*'), p.bin, {
            permissions: 0755, 
            exclude: /file-save|www|simple|sample/,
        })
    }

    if (bit.ssl && bit.platform.os == 'linux') {
        install(bit.dir.bin.join('*.' + bit.ext.shobj + '*'), p.bin, {strip: strip, permissions: 0755})
        for each (f in p.bin.files('*.so.*')) {
            let withver = f.basename
            let nover = withver.name.replace(/\.[0-9]*.*/, '.so')
            let link = p.bin.join(nover)
            f.remove()
            f.symlink(link.basename)
        }
    }
    if (!bit.cross) {
        if (bit.platform.os == 'macosx') {
            /* KEEP
            let daemons = contents.join('Library/LaunchDaemons')
            daemons.makeDir()
            install('package/MACOSX/com.embedthis.goahead.plist', daemons, {permissions: 0644, expand: true})
             */

        } else if (bit.platform.os == 'linux') {
            /* KEEP
            install('package/LINUX/' + settings.product + '.init', contents.join('etc/init.d', settings.product), 
                {permissions: 0755, expand: true})
            install('package/LINUX/' + settings.product + '.upstart', 
                contents.join('init', settings.product).joinExt('conf'),
                {permissions: 0644, expand: true})
            */

        } else if (bit.platform.os == 'windows') {
            let version = bit.packs.compiler.version.replace('.', '')
            if (bit.platform.arch == 'x64') {
                install(bit.packs.compiler.dir.join('VC/redist/x64/Microsoft.VC' + 
                    version + '.CRT/msvcr' + version + '.dll'), p.bin)
            } else {
                install(bit.packs.compiler.dir.join('VC/redist/x86/Microsoft.VC' + 
                    version + '.CRT/msvcr' + version + '.dll'), p.bin)
            }
            if (!minimal) {
                install(bit.dir.bin.join('removeFiles' + bit.globals.EXE), p.bin)
            }
        }
        if (bit.platform.like == 'posix' && !minimal) {
            install('doc/man/*.1', p.productver.join('doc/man/man1'), {compress: true})
        }
    }
    if (!minimal) {
        let files = contents.files('**', {exclude: /\/$/, relative: true})
        files = files.map(function(f) Path("/" + f))
        p.productver.join('files.log').append(files.join('\n') + '\n')
    }
    if (formats && bit.platform.last) {
        package(bit.dir.pkg.join('bin'), formats)
    }
}

public function installBinary() {
    if (Config.OS != 'windows' && App.uid != 0) {
        throw 'Must run as root. Use \"sudo bit install\"'
    }
    packageBinaryFiles(null, true)
    package(bit.dir.pkg.join('bin'), 'install')
    if (!bit.cross) {
        if (Config.OS != 'windows') {
            createLinks()
            updateLatestLink()
        }
    }
    if (!bit.options.keep) {
        bit.dir.pkg.join('bin').removeAll()
    } else {
        trace('Keep', bit.dir.pkg.join('bin'))
    }
    trace('Complete', bit.settings.title + ' installed')
}

public function uninstallBinary() {
    if (Config.OS != 'windows' && App.uid != 0) {
        throw 'Must run as root. Use \"sudo bit install\"'
    }
    trace('Uninstall', bit.settings.title)                                                     
    if (bit.platform.os == 'windows') {
        Cmd([bit.prefixes.bin.join('goaheadMonitor'), '--stop'])
    }
    let fileslog = bit.prefixes.productver.join('files.log')
    if (fileslog.exists) {
        for each (let file: Path in fileslog.readLines()) {
            vtrace('Remove', file)
            file.remove()
        }
    }
    fileslog.remove()
    for each (file in bit.prefixes.log.files('*.log*')) {
        file.remove()
    }
    for each (prefix in bit.prefixes) {
        for each (dir in prefix.files('**', {include: /\/$/}).sort().reverse()) {
            vtrace('Remove', dir)
            dir.remove()
        }
        vtrace('Remove', prefix)
        prefix.remove()
    }
    updateLatestLink()
}

/*
    Create symlinks for binaries and man pages
 */
public function createLinks() {
    let log = []
    let localbin = Path('/usr/local/bin')
    if (localbin.exists) {
        let programs = ['goahead']
        let bin = bit.prefixes.bin
        let target: Path
        for each (program in programs) {
            let link = Path(localbin.join(program))
            link.symlink(bin.join(program + bit.globals.EXE))
            log.push(link)
        }
        for each (page in bit.prefixes.productver.join('doc/man').files('**/*.1.gz')) {
            let link = Path('/usr/share/man/man1/' + page.basename)
            link.symlink(page)
            log.push(link)
        }
    }
    if (Config.OS != 'windows') {
        let link = Path('/usr/include').join(bit.settings.product)
        link.symlink(bit.prefixes.inc)
        log.push(link)
        bit.prefixes.productver.join('files.log').append(log.join('\n') + '\n')
    }
}

function updateLatestLink() {
    let latest = bit.prefixes.product.join('latest')
    let version = bit.prefixes.product.files('*', {include: /\d+\.\d+\.\d+/}).sort().pop()
    if (version) {
        latest.symlink(version.basename)
    } else {
        latest.remove()
    }
}


public function packageSourceFiles() {
    if (bit.cross) {
        return
    }
    let s = bit.settings
    let src = bit.dir.pkg.join('src')
    let pkg = src.join(s.product + '-' + s.version)
    safeRemove(pkg)
    pkg.makeDir()
    install(['Makefile', 'start.bit', 'main.bit'], pkg)
    install('bits', pkg)
    install('configure', pkg, {permissions: 0755})
    install('*.md', pkg, {fold: true, expand: true})
    install(['src/*.c', 'src/*.h'], pkg, {
        exclude: /\.log$|\.lst$|\/utils|ejs.zip|\.stackdump$|\/cache\/|huge.txt|\.swp$|\.tmp/,
    })
    install('src/utils', pkg, {
        exclude: /\.log$|\.lst$|ejs.zip|\.stackdump$|\/cache\/|huge.txt|\.swp$|\.tmp|\.o$|\.obj$|\.so$|\.dylib$/,
    })
    install('test', pkg, {
        exclude: /\.log$|\.lst$|ejs.zip|\.stackdump$|\/cache\/|huge.txt|\.swp$|\.tmp|\.tdat$|\.o$|\.obj$|\.so$|\.dylib$/,
    })
    install('doc', pkg, {
        exclude: /\/xml\/|\/html\/|Archive|\.mod$|\.so$|\.dylib$|\.o$/,
    })
    install('projects', pkg, {
        exclude: /\/Debug\/|\/Release\/|\.ncb|\.mode1v3|\.pbxuser/,
    })
    install('package', pkg, {})
    package(bit.dir.pkg.join('src'), 'src')
}

public function packageComboFiles() {
    if (bit.cross) {
        return
    }
    let s = bit.settings
    let src = bit.dir.pkg.join('src')
    let pkg = src.join(s.product + '-' + s.version)
    safeRemove(pkg)
    pkg.makeDir()
    install('projects/goahead-' + bit.platform.os + '-default-bit.h', pkg.join('src/deps/goahead/bit.h'))
    install('package/start-flat.bit', pkg.join('src/deps/goahead/start.bit'))
    install('package/Makefile-flat', pkg.join('src/deps/goahead/Makefile'))
    install(['src/js.h', 'src/goahead.h'], 
        pkg.join('src/deps/goahead/goahead.h'), {
        cat: true,
        filter: /^#inc.*goahead.*$/mg,
        title: bit.settings.title + ' Library Source',
    })
    install(['src/*.c'], pkg.join('src/deps/goahead/goaheadLib.c'), {
        cat: true,
        filter: /^#inc.*goahead.*$|^#inc.*mpr.*$|^#inc.*http.*$|^#inc.*customize.*$|^#inc.*edi.*$|^#inc.*mdb.*$|^#inc.*esp.*$/mg,
        exclude: /goahead.c|samples|test.c/,
        header: '#include \"goahead.h\"',
        title: bit.settings.title + ' Library Source',
    })
    install(['src/goahead.c'], pkg.join('src/deps/goahead/goahead.c'))
    install(['src/route.txt'], pkg.join('src/deps/goahead/route.txt'))
    install(['src/auth.txt'], pkg.join('src/deps/goahead/auth.txt'))
    package(bit.dir.pkg.join('src'), ['combo', 'flat'])
}

/*
    @copy   default
  
    Copyright (c) Embedthis Software LLC, 2003-2013. All Rights Reserved.
  
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis GoAhead Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
  
    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
