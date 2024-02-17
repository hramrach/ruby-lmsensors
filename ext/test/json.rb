require 'lmsensors'

sensors = LMSensors.new('/dev/null')

STDOUT.puts '{'
has_chips = false
sensors.each{|chip|
  STDOUT.write ',' if has_chips
  has_chips = true
  has_features = false
  STDOUT.puts "\"#{chip.name}\":{"
  adapter = chip.adapter
  STDOUT.puts "\"Adapter\":\"#{adapter}\"" if adapter
  chip.each{|ftr|
    STDOUT.write ',' if has_features || adapter
    has_features = true
    has_subfeatures = false
    STDOUT.puts "\"#{ftr.name}\":{"
    label = (ftr.name != ftr.label) ? ftr.label : nil
    STDOUT.puts "\"label\":\"#{ftr.label}\"" if label
    ftr.each{|sub|
      STDOUT.write ',' if has_subfeatures || label
      has_subfeatures = true
      STDOUT.puts "\"#{sub.label}\":{"
      quantity = sub.quantity.gsub(/^.* /, '')
      STDOUT.puts "\"quantity\":\"#{quantity}\""
      if sub.unit.length > 0 then
        STDOUT.write ','
        STDOUT.puts "\"unit\":\"#{sub.unit}\""
      end
      STDOUT.write ','
      STDOUT.puts "\"value\":#{sub.value}"
      STDOUT.puts '}'
    }
    STDOUT.puts '}'
  }
  STDOUT.puts '}'
}
STDOUT.puts '}'
