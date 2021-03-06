# Configuration file for Mirb

class MSpecScript
  # Language features specs
  set :language, [ 'rubyspec/language' ]

  # Core library specs
  set :core, [
    'rubyspec/core',
	
    '^rubyspec/core/rational',
    '^rubyspec/core/signal',
    '^rubyspec/core/basicobject',
    '^rubyspec/core/argf',
    '^rubyspec/core/env',
    '^rubyspec/core/complex',
    '^rubyspec/core/continuation',
    '^rubyspec/core/binding',
    '^rubyspec/core/objectspace',
    '^rubyspec/core/fiber',
    '^rubyspec/core/threadgroup',
    '^rubyspec/core/thread',
    '^rubyspec/core/mutex',
    '^rubyspec/core/process'
  ]
  
  # An ordered list of the directories containing specs to run
  set :files, get(:language) + get(:core)

  # This set of files is run by mspec ci
  set :ci_files, get(:files)

  set :tags_patterns, [
                        [%r(rubyspec/),     'tags/'],
                        [/_spec.rb$/,       '_tags.txt']
                      ]
end
