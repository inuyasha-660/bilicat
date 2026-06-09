project('bilicat', 'c',
    default_options: [ 
        'buildtype=release'
  ],)

dep = [
  dependency('libcurl'),
  dependency('yyjson'),
  ]

src = files(
  'main.c',
  'api.c',
  'libs/log.c',
  'libs/argparse.c',
  )

executable('bilicat', src, dependencies: dep)
