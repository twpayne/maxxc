#!/usr/bin/ruby

# TODO embed IGC file in GPX
# TODO read tracklog from GPX
# TODO arrows on route
# TODO icon styles for waypoints
# TODO nice colours
# TODO summary table with leg percentages
# TODO loop gap

require "enumerator"
require "optparse"
require "rexml/document"

class Symbol

  def to_proc
    lambda { |object| object.send(self) }
  end

end

module KML

  class Element

    def initialize(tag)
      @tag = tag
    end

  end

  class Simple < Element

    def initialize(tag, text = nil)
      super(tag)
      @text = text
    end

    def write(io)
      if @text
        io.write("<#{@tag}>#{@text}</#{@tag}>")
      else
        io.write("<#{@tag}/>")
      end
    end

  end

  class E < Element

    def initialize(tag, *args)
      super(tag)
      @children = []
      args.each do |arg|
        case arg
        when Hash then arg.each { |key, value| @children << Simple.new(key, value) }
        else @children << arg
        end
      end
    end

    def <<(child)
      @children << child
      self
    end

    def write(io)
      io.write("<#{@tag}")
      if @children.empty?
        io.write("/>")
      else
        io.write(">")
        @children.each { |child| child.write(io) }
        io.write("</#{@tag}>")
      end
    end

  end

end

class Coord

  R = 6371.0

  attr_reader :lat, :lon, :ele

  def initialize(lat, lon, ele)
    @lat, @lon, @ele = lat, lon, ele
  end

  def to_kml
    "%f,%f,%f" % [@lon, @lat, @ele]
  end

  def distance_to(coord)
    lat1 = Math::PI * @lat.to_f / 180.0
    lon1 = Math::PI * @lon.to_f / 180.0
    lat2 = Math::PI * coord.lat.to_f / 180.0
    lon2 = Math::PI * coord.lon.to_f / 180.0
    x = Math.sin(lat1) * Math.sin(lat2) + Math.cos(lat1) * Math.cos(lat2) * Math.cos(lon1 - lon2)
    x < 1.0 ? R * Math.acos(x) : 0.0
  end

  def halfway_to(coord)
    lat1 = Math::PI * @lat.to_f / 180.0
    lon1 = Math::PI * @lon.to_f / 180.0
    lat2 = Math::PI * coord.lat.to_f / 180.0
    lon2 = Math::PI * coord.lon.to_f / 180.0
    bx = Math.cos(lat2) * Math.cos(lon2 - lon1)
    by = Math.cos(lat2) * Math.sin(lon2 - lon1)
    cos_lat1_plus_bx = Math.cos(lat1) + bx
    lat3 = Math.atan2(Math.sin(lat1) + Math.sin(lat2), Math.sqrt(cos_lat1_plus_bx * cos_lat1_plus_bx + by * by))
    lon3 = lon1 + Math.atan2(by, cos_lat1_plus_bx)
    Coord.new(180.0 * lat3 / Math::PI, 180.0 * lon3 / Math::PI, (@ele.to_f + coord.ele.to_f) / 2.0)
  end

end

class GPX

  class Wpt

    attr_reader :coord

    def initialize(wpt)
      lat = wpt.attributes["lat"]
      lon = wpt.attributes["lon"]
      @coord = Coord.new(lat, lon, 0)
      @name = wpt.elements["name"].text
      @time = wpt.elements["time"].text
    end

    def to_kml
      placemark = KML::E.new(:Placemark, :name => @name)
      placemark << KML::E.new(:Point, :coordinates => @coord.to_kml)
    end

  end

  class Rte

    attr_reader :score

    def initialize(rte)
      @name = rte.elements["name"].text
      @distance = rte.elements["extensions/distance"].text
      @score = rte.elements["extensions/score"].text
      @multiplier = rte.elements["extensions/multiplier"].text
      @circuit = rte.elements["extensions/circuit"] and true
      @declared = rte.elements["extensions/declared"] and true
      @rtepts = []
      rte.elements.each("rtept") { |rtept| @rtepts << Wpt.new(rtept) }
    end

    def to_kml
      folder = KML::E.new(:Folder,
        :name => "#{@name} (#{@score} points, #{@distance}km)",
        :Snippet => nil,
        :description => 
          "<![CDATA[" +
          "<table>" +
          "<tr><td>Distance</td><td>#{@distance}km</td></tr>" +
          "<tr><td>Multiplier</td><td>\xc3\x97 #{@multiplier} points/km</td></tr>" +
          "<tr><td>Score</td><td>#{@score} points</td></tr>" +
          "</table>" +
          "]]>")
      folder << (KML::E.new(:Style) << KML::E.new(:ListStyle, :listItemType => :checkHideChildren))
      @rtepts.each do |rtept|
        folder << rtept.to_kml
      end
      if @circuit
        coords = (@rtepts[1...-1] << @rtepts[1]).collect(&:coord)
      else
        coords = @rtepts.collect(&:coord)
      end
      route = KML::E.new(:Placemark)
      route << (KML::E.new(:Style) << KML::E.new(:LineStyle, :width => 2))
      route << KML::E.new(:LineString, :altitudeMode => :clampToGround, :tessellate => 1, :coordinates => coords.collect(&:to_kml).join(" "))
      folder << route
      coords.each_cons(2) do |coord1, coord2|
        placemark = KML::E.new(:Placemark, :name => "%.2fkm" % coord1.distance_to(coord2))
        placemark << KML::E.new(:Point, :coordinates => coord1.halfway_to(coord2).to_kml)
        folder << placemark
      end
      if @circuit
        start = KML::E.new(:Placemark)
        start << KML::E.new(:LineString, :altitudeMode => :clampToGround, :tessellate => 1, :coordinates => [@rtepts[0], @rtepts[1]].collect(&:coord).collect(&:to_kml).join(" "))
        folder << start
        finish = KML::E.new(:Placemark)
        finish << KML::E.new(:LineString, :altitudeMode => :clampToGround, :tessellate => 1, :coordinates => [@rtepts[-2], @rtepts[-1]].collect(&:coord).collect(&:to_kml).join(" "))
        folder << finish
      end
      folder
    end

  end

  def initialize(io)
    document = REXML::Document.new(io)
    @league = document.elements["gpx/metadata/extensions/league"].text
    @rtes = []
    document.elements.each("gpx/rte") { |rte| @rtes << Rte.new(rte) }
    @rtes = @rtes.sort_by { |rte| -rte.score.to_f }
  end

  def to_kml
    folder = KML::E.new(:Folder, :name => @league, :open => 1)
    folder << (KML::E.new(:Style) << KML::E.new(:ListStyle, :listItemType => :radioFolder))
    @rtes.each { |rte| folder << rte.to_kml }
    folder
  end

end

class IGC

  def initialize(io)
    @name = io.path
    @coords = []
    mday = mon = year = nil
    io.each do |line|
      case line
      when /\AHFDTE(\d\d)(\d\d)(\d\d)\r\n\z/
        mday = $1.to_i
        mon = $2.to_i
        year = 2000 + $3.to_i
      when /\AB(\d\d)(\d\d)(\d\d)(\d\d)(\d{5})([NS])(\d\d\d)(\d{5})([EW])([AV])(\d{5})(\d{5})\d+\r\n\z/
        time = Time.utc(year, mon, mday, $1.to_i, $2.to_i, $3.to_i)
        lat = $4.to_i + $5.to_i / 60000.0
        lat = -lat if $6 == "S"
        lon = $7.to_i + $8.to_i / 60000.0
        lon = -lon if $9 == "W"
        ele = $12.to_i
        @coords << Coord.new(lat, lon, ele)
      end
    end
  end

  def to_kml
    folder = KML::E.new(:Folder, :name => @name, :open => 1)
    folder << (KML::E.new(:Style) << KML::E.new(:ListStyle, :listItemType => :radioFolder))
    tracklog_2d = KML::E.new(:Placemark, :name => "2D tracklog")
    tracklog_2d << (KML::E.new(:Style) << KML::E.new(:LineStyle, :color => "ff0000cc", :width => 2))
    coordinates = @coords.collect(&:to_kml).join(" ")
    tracklog_2d << KML::E.new(:LineString, :altitudeMode => :clampToGroup, :coordinates => coordinates)
    folder << tracklog_2d
    folder_3d = KML::E.new(:Folder, :name => "3D tracklog")
    folder_3d << (KML::E.new(:Style) << KML::E.new(:ListStyle, :listItemType => :checkHideChildren))
    tracklog_3d = KML::E.new(:Placemark)
    tracklog_3d << (KML::E.new(:Style) << KML::E.new(:LineStyle, :color => "ff0000cc", :width => 2))
    tracklog_3d << KML::E.new(:LineString, :altitudeMode => :absolute, :coordinates => coordinates)
    folder_3d << tracklog_3d
    shadow = KML::E.new(:Placemark)
    shadow << (KML::E.new(:Style) << KML::E.new(:LineStyle, :color => "ff000000"))
    shadow << KML::E.new(:LineString, :altitudeMode => :clampToGround, :coordinates => coordinates)
    folder_3d << shadow
    folder << folder_3d
  end

end

def main(argv)
  output_filename = nil
  OptionParser.new do |op|
    op.on("--help", "print usage and exit") do
      print(op)
      return
    end
    op.on("--output=FILENAME", String, "output filename") do |arg|
      output_filename = arg
    end
    op.parse!(argv)
  end
  document = KML::E.new(:Document)
  document << GPX.new($stdin).to_kml
  argv.each do |arg|
    case arg
    when /\.igc\z/i then File.open(arg) { |io| document << IGC.new(io).to_kml }
    else raise arg
    end
  end
  kml = KML::E.new(:kml) #, :xmlns => "http://earth.google.com/kml/2.1")
  kml << document
  if !output_filename or output_filename == "-"
    $stdout.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")
    kml.write($stdout)
  else
    File.open(output_filename, "w") do |output|
      output.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")
      kml.write(output)
    end
  end
end

exit(main(ARGV) || 0) if $0 == __FILE__
