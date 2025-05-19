project('barbar', 'c',
  default_options : ['c_std=c11', 'warning_level=3', 'b_lundef=true'])

util_sources = ['util.c']
inc_dirs = include_directories('.', 'modules')
module_source_files = [
  'bartime.c',
  'ccq_dues.c',
  'dailies.c',
  'get_ip.c',
  'pomodoro.c'
]

# barbar
executable('barbar',
  ['barbar.c'] + util_sources,
  include_directories : inc_dirs,
  install : true
)

# modules
foreach module_src : module_source_files
  module_name = module_src.split('.')[0] 
  executable(module_name,
    [files('modules/' + module_src)] + util_sources,
    include_directories : inc_dirs,
    install : true
  )
endforeach

# man page
install_man('barbar.1')
