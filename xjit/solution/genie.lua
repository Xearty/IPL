if not solution() then
    solution 'spasm'
    configurations {'Debug', 'Release'}
    platforms { 'x64' }
    flags {
        'FatalWarnings',
        'ExtraWarnings',
        'Cpp14',
        'Symbols',
    }

    defines {
        '_SCL_SECURE_NO_WARNINGS',
    }

    local root = '../build/'

    if _ACTION ~= 'ninja' then
        location '.'
    else
        location './ninja'
    end

    configuration 'Debug'
        targetdir(root .. 'bin/Debug')
        objdir(root .. 'obj/Debug')

    configuration 'Release'
        flags 'OptimizeSpeed'
        targetdir(root .. 'bin/Release')
        objdir(root .. 'obj/Release')
    configuration '*'
end
    project 'xjit_lib'
        kind 'StaticLib'
        language 'C++'
        uuid(os.uuid('xjit_lib'))
        location(solution().location)
        links { 'spasm_lib', 'JSLib', 'sprt' }
        -- files '../src/xjit/jit.cpp'
        files '../src/xjit/**.cpp'
        files '../src/**/*.h'
        includedirs { '../../JSImpl/src/', '../../spasm/src/' }

    project 'xjit'
        kind 'ConsoleApp'
        language 'C++'
        uuid(os.uuid('xjit'))
        location(solution().location)
		links 'xjit_lib'
        files '../src/main.cpp'
        files '../src/xjit/**/*.h'
        includedirs { '../../JSImpl/src/', '../../spasm/src/' }

    startproject 'xjit'