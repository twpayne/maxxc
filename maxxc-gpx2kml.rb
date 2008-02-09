#!/usr/bin/ruby

# TODO embed IGC file in GPX
# TODO embed IGC filename in GPX
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

class KML

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
      io.write(@text ? "<#{@tag}>#{@text}</#{@tag}>" : "<#{@tag}/>")
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
      if @children.empty?
        io.write("<#{@tag}/>")
      else
        io.write("<#{@tag}>")
        @children.each { |child| child.write(io) }
        io.write("</#{@tag}>")
      end
    end

  end

  def initialize(root)
    @root = root
  end

  def write(io)
    io.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")
    io.write("<kml xmlns=\"http://earth.google.com/kml/2.1\">")
    @root.write(io)
    io.write("</kml>")
  end

end

class Coord

  R = 6371.0

  attr_reader :lat, :lon, :ele

  def initialize(lat, lon, ele)
    @lat, @lon, @ele = lat, lon, ele
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

  def to_kml
    "#{@lon},#{@lat},#{@ele}"
  end

end

class GPX

  class Trk

    def initialize(trk)
      @trkpts = []
      trk.elements.each("trkseg/trkpt") { |trkpt| @trkpts << TrkPt.new(trkpt) }
    end

    def to_kml
      folder = KML::E.new(:Folder, :name => "Track", :open => 1)
      folder << (KML::E.new(:Style) << KML::E.new(:ListStyle, :listItemType => :radioFolder))
      track_2d = KML::E.new(:Placemark, :name => "2D", :visibility => 0)
      track_2d << (KML::E.new(:Style) << KML::E.new(:LineStyle, :color => "ff0000cc", :width => 2))
      coordinates = @trkpts.collect(&:to_kml).join(" ")
      track_2d << KML::E.new(:LineString, :altitudeMode => :clampToGroup, :coordinates => coordinates)
      folder << track_2d
      folder_3d = KML::E.new(:Folder, :name => "3D")
      folder_3d << (KML::E.new(:Style) << KML::E.new(:ListStyle, :listItemType => :checkHideChildren))
      track_3d = KML::E.new(:Placemark)
      track_3d << (KML::E.new(:Style) << KML::E.new(:LineStyle, :color => "ff0000cc", :width => 2))
      track_3d << KML::E.new(:LineString, :altitudeMode => :absolute, :coordinates => coordinates)
      folder_3d << track_3d
      shadow = KML::E.new(:Placemark)
      shadow << (KML::E.new(:Style) << KML::E.new(:LineStyle, :color => "ff000000"))
      shadow << KML::E.new(:LineString, :altitudeMode => :clampToGround, :coordinates => coordinates)
      folder_3d << shadow
      folder << folder_3d
    end

  end

  class TrkPt

    def initialize(trkpt)
      @lat = trkpt.attributes["lat"]
      @lon = trkpt.attributes["lon"]
      @ele = trkpt.elements["ele"] ? trkpt.elements["ele"].text : 0
    end

    def to_kml
      "#{@lon},#{@lat},#{@ele || 0}"
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

    def to_kml(options = {})
      folder = KML::E.new(:Folder,
        options,
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
      @rtepts.each { |rtept| folder << rtept.to_kml }
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

  def initialize(io)
    @name = nil
    document = REXML::Document.new(io)
    @league = document.elements["gpx/metadata/extensions/league"].text
    @rtes = []
    document.elements.each("gpx/rte") { |rte| @rtes << Rte.new(rte) }
    @rtes = @rtes.sort_by { |rte| -rte.score.to_f }
    @trk = Trk.new(document.elements["gpx/trk"]) if document.elements["gpx/trk"]
  end

  def to_kml
    folder = KML::E.new(:Folder, :name => @league, :open => 1)
    rte_folder = KML::E.new(:Folder, :name => "Routes", :open => 1)
    rte_folder << (KML::E.new(:Style) << KML::E.new(:ListStyle, :listItemType => :radioFolder))
    visibility = 1
    @rtes.each do |rte|
      rte_folder << rte.to_kml(:visibility => visibility)
      visibility = 0
    end
    folder << rte_folder
    folder << @trk.to_kml if @trk
    folder
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
  kml = KML.new(KML::E.new(:Document, GPX.new($stdin).to_kml))
  if !output_filename or output_filename == "-"
    kml.write($stdout)
  else
    File.open(output_filename, "w") { |io| kml.write(io) }
  end
end

exit(main(ARGV) || 0) if $0 == __FILE__
