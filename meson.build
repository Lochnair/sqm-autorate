project('sqm-autorate', 'c')

thread_dep = dependency('threads')

sources = [
    'src/main.c',
    'src/pinger.c',
    'src/pinger_icmp.c',
    'src/pinger_udp.c',
    'src/utils.c'
]

executable('sqm-autorate', sources, dependencies: thread_dep)
