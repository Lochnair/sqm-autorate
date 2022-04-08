project('sqm-autorate', 'c')


cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)
thread_dep = dependency('threads')

deps = [
    m_dep,
    thread_dep
]

add_project_arguments('-D_GNU_SOURCE', language: 'c')

sources = [
    'src/main.c',
    'src/baseliner.c',
    'src/hash_table.c',
    'src/log.c',
    'src/pinger.c',
    'src/pinger_icmp.c',
    'src/pinger_udp.c',
    'src/prime.c',
    'src/pthread_ext_common.c',
    'src/pthread_queue.c',
    'src/ratecontroller.c',
    'src/reflectors.c',
    'src/settings.c',
    'src/utils.c'
]

executable('sqm-autorate', sources, dependencies: deps)
