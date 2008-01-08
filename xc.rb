require "coord"
require "enumerator"
require "fileutils"
require "yaml"

module XC

  LEAGUES = []

  class League

    CACHE_DIRECTORY = File.join("tmp", "xc");

    class << self

      def inherited(klass)
        LEAGUES << klass
      end

      def description
        const_get(:DESCRIPTION)
      end

      def minimum_distance
        const_get(:MINIMUM_DISTANCE)
      end

      def turnpoint_name(index, length)
        case index
        when 0 then const_get(:TURNPOINT_START_NAME)
        when length - 1 then const_get(:TURNPOINT_FINISH_NAME)
        else const_get(:TURNPOINT_NAME) % index
        end
      end

      def memoized_optimize(key, fixes)
        memofile = File.join(CACHE_DIRECTORY, name.split(/::/)[-1], key)
        if FileTest.exist?(memofile) and !FileTest.zero?(memofile)
          File.open(memofile) do |file|
            hash = YAML.load(file)
            ts = fixes.collect(&:time).collect!(&:to_i)
            hash.collect do |type, times|
              turnpoints = times.collect do |time|
                fixes[ts.find_first_ge(time)]
              end
              const_get(type).new(turnpoints)
            end
          end
        else
          xcs = optimize(fixes)
          hash = {}
          xcs.each do |xc|
            hash[xc.class.name.split(/::/)[-1]] = xc.turnpoints.collect(&:time).collect!(&:to_i)
          end
          FileUtils.mkdir_p(File.dirname(memofile))
          File.open(memofile, "w") do |file|
            file.write(hash.to_yaml)
          end
          xcs
        end
      end

    end

  end

  class Turnpoint < Coord

    attr_reader :name
    attr_reader :time

    def initialize(lat, lon, alt, time, name)
      super(lat, lon, alt)
      @time = time
      @name = name
    end

  end

  class Flight

    MULTIPLIER = 1.0
    CIRCUIT = false

    attr_reader :distance
    attr_reader :league
    attr_reader :score
    attr_reader :turnpoints

    def initialize(fixes)
      raise unless fixes.length == self.class.const_get(:TURNPOINTS) + 2
      @league = self.class.module_heirarchy[1]
      @turnpoints = fixes.collect_with_index do |fix, index|
        Turnpoint.new(fix.lat, fix.lon, fix.alt, fix.time, @league.turnpoint_name(index, fixes.length))
      end
      @distance = 0.0
      if circuit?
        @turnpoints[1...-1].each_cons(2) do |tp0, tp1|
          @distance += tp0.distance_to(tp1)
        end
        @distance += @turnpoints[-2].distance_to(@turnpoints[1])
      else
        @turnpoints.each_cons(2) do |tp0, tp1|
          @distance += tp0.distance_to(tp1)
        end
      end
      @score = multiplier * @distance / 1000.0
    end

    def circuit?
      self.class.const_get(:CIRCUIT)
    end

    def multiplier
      distance < @league.minimum_distance ? 0.0 : self.class.const_get(:MULTIPLIER)
    end

    def type
      self.class.const_get(:TYPE)
    end

  end

  class << self

    def league(league_class, description, minimum_distance, turnpoint_start_name, turnpoint_name, turnpoint_finish_name, flight_types = {})
      flight_class_declarations = flight_types.collect do |flight_class, values|
        ["class #{flight_class} < Flight", values.collect { |k, v| "#{k.to_s.upcase} = #{v.inspect}" }.join("\n"), "end"].join("\n")
      end
      class_eval <<-EOC
        class #{league_class} < League
          DESCRIPTION = #{description.inspect}
          MINIMUM_DISTANCE = #{minimum_distance.inspect}
          TURNPOINT_START_NAME = #{turnpoint_start_name.inspect}
          TURNPOINT_NAME = #{turnpoint_name.inspect}
          TURNPOINT_FINISH_NAME = #{turnpoint_finish_name.inspect}
          #{flight_class_declarations.join("\n")}
        end
      EOC
    end

  end

  league :Open, nil, 0.0, "Start", "TP%d", "Finish", {
    :Open0 => { :turnpoints => 0, :type => "Open distance", :multiplier => 0.0 },
  }

  league :FRCFD, "Coupe F\xc3\xa9d\xc3\xa9rale de Distance (France)", 15000.0, "BD", "B%d", "BA", {
    :Open0       => { :turnpoints => 0, :type => "Distance libre"                                                                        },
    :Open1       => { :turnpoints => 1, :type => "Distance libre avec un point de contournement"                                         },
    :Open2       => { :turnpoints => 2, :type => "Distance libre avec deux points de contournement"                                      },
    :Circuit2    => { :turnpoints => 2, :type => "Parcours en aller-retour",                        :circuit => true, :multiplier => 1.2 },
    :Circuit3    => { :turnpoints => 3, :type => "Triangle plat",                                   :circuit => true, :multiplier => 1.2 },
    :Circuit3FAI => { :turnpoints => 3, :type => "Triangle FAI",                                    :circuit => true, :multiplier => 1.4 },
    :Circuit4    => { :turnpoints => 4, :type => "Quadrilat\xc3\xa8re",                             :circuit => true, :multiplier => 1.2 },
  }

  league :UKXCL, "Cross Country League (UK)", 15000.0, "Start", "TP%d", "Finish", {
    :Open0       => { :turnpoints => 0, :type => "Open distance"                                                           },
    :Open1       => { :turnpoints => 1, :type => "Open distance via a turnpoint"                                           },
    :Open2       => { :turnpoints => 2, :type => "Open distance via two turnpoints"                                        },
    :Open3       => { :turnpoints => 3, :type => "Open distance via three turnpoints"                                      },
    :Circuit2    => { :turnpoints => 2, :type => "Out and return",                    :circuit => true, :multiplier => 2.0 },
    :Circuit3    => { :turnpoints => 3, :type => "Flat triangle",                     :circuit => true, :multiplier => 2.0 },
    :Circuit3FAI => { :turnpoints => 3, :type => "FAI triangle",                      :circuit => true, :multiplier => 3.0 },
  }

=begin
  league :HOLC, "Onlinecontest", 15000.0, "Start", "TP%d", "Finish", {
    :Open3       => { :turnpoints => 3, :type => "Open distance via three turnpoints",                  :multiplier => 1.5  },
    :Circuit3    => { :turnpoints => 3, :type => "Flat triangle",                     :circuit => true, :multiplier => 1.75 },
    :Circuit3FAI => { :turnpoints => 3, :type => "FAI triangle",                      :circuit => true, :multiplier => 2.0  },
  }
=end

  class << self

    def leagues_hash
      result = {}
      LEAGUES.each do |league|
        result[league.name.split(/::/)[-1]] = league
      end
      result
    end

  end

end

require "cxc"
