#!/usr/bin/ruby

# TODO embed IGC file in GPX
# TODO embed IGC filename in GPX

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
      @attributes = {}
      args.each do |arg|
        case arg
        when Hash
          arg.each do |key, value|
            case key
            when :attributes then @attributes.merge!(value)
            else @children << Simple.new(key, value)
            end
          end
        else
          @children << arg
        end
      end
    end

    def <<(child)
      @children << child
      self
    end

    def write(io)
      io.write("<#{@tag}")
      @attributes.each do |key, value|
        io.write(" #{key}=\"#{value}\"")
      end
      if @children.empty?
        io.write("/>")
      else
        io.write(">")
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
    @root.write(io)
  end

end

class Coord

  R = 6371.0

  attr_reader :lat, :lon, :ele

  def initialize(lat, lon, ele)
    @lat, @lon, @ele = lat, lon, ele
  end

  def coord_at(bearing, distance)
    lat1 = Math::PI * @lat.to_f / 180.0
    lon1 = Math::PI * @lon.to_f / 180.0
    d_div_R = distance / R
    lat2 = Math.asin(Math.sin(lat1) * Math.cos(d_div_R) + Math.cos(lat1) * Math.sin(d_div_R) * Math.cos(bearing))
    lon2 = lon1 + Math.atan2(Math.sin(bearing) * Math.sin(d_div_R) * Math.cos(lat1), Math.cos(d_div_R) - Math.sin(lat1) * Math.sin(lat2))
    Coord.new(180 * lat2 / Math::PI, 180.0 * lon2 / Math::PI, @ele)
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

  def initial_bearing_to(coord)
    lat1 = Math::PI * @lat.to_f / 180.0
    lon1 = Math::PI * @lon.to_f / 180.0
    lat2 = Math::PI * coord.lat.to_f / 180.0
    lon2 = Math::PI * coord.lon.to_f / 180.0
    Math.atan2(Math.sin(lon2 - lon1) * Math.cos(lat2), Math.cos(lat1) * Math.sin(lat2) - Math.sin(lat1) * Math.cos(lat2) * Math.cos(lon2 - lon1))
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
      track_2d = KML::E.new(:Placemark, :name => "2D", :visibility => 0, :styleUrl => :track)
      coordinates = @trkpts.collect(&:to_kml).join(" ")
      track_2d << KML::E.new(:LineString, :altitudeMode => :clampToGroup, :coordinates => coordinates)
      folder << track_2d
      folder_3d = KML::E.new(:Folder, :name => "3D")
      folder_3d << (KML::E.new(:Style) << KML::E.new(:ListStyle, :listItemType => :checkHideChildren))
      track_3d = KML::E.new(:Placemark, :styleUrl => :track)
      track_3d << KML::E.new(:LineString, :altitudeMode => :absolute, :coordinates => coordinates)
      folder_3d << track_3d
      shadow = KML::E.new(:Placemark, :styleUrl => :shadow)
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
      folder = KML::E.new(:Folder, options, :name => "#{@name} (#{@score} points, #{@distance}km)", :Snippet => nil)
      folder << (KML::E.new(:Style) << KML::E.new(:ListStyle, :listItemType => :checkHideChildren))
      rows = []
      if @circuit
        @rtepts[1...-1].each_cons(2) do |rtept1, rtept2|
          leg_distance = rtept1.distance_to(rtept2)
          rows << ["#{rtept1.name} \xe2\x86\x92 #{rtept2.name}", "%.3fkm (%.1f%%)" % [leg_distance, 100.0 * leg_distance / @distance.to_f]]
        end
        leg_distance = @rtepts[-2].distance_to(@rtepts[1])
        rows << ["#{@rtepts[-2].name} \xe2\x86\x92 #{@rtepts[1].name}", "%.3fkm (%.1f%%)" % [leg_distance, 100.0 * leg_distance / @distance.to_f]]
        rows << ["#{@rtepts[-1].name} \xe2\x86\x92 #{@rtepts[0].name}", "%.3fkm" % @rtepts[-1].distance_to(@rtepts[0])]
      else
        @rtepts.each_cons(2) do |rtept1, rtept2|
          rows << ["#{rtept1.name} \xe2\x86\x92 #{rtept2.name}", "%.3fkm" % rtept1.distance_to(rtept2)]
        end
      end
      rows << ["Distance", "#{@distance}km"]
      rows << ["Multiplier", "\xc3\x97 #{@multiplier}/km"]
      rows << ["Score", "<b>#{@score}</b>"]
      folder << KML::Simple.new(:description, "<![CDATA[<table>" + rows.collect { |cells| "<tr>" + cells.collect { |cell| "<td>#{cell}</td>" }.join + "</tr>" }.join + "</table>]]>")
      @rtepts.each { |rtept| folder << rtept.to_kml }
      coords = (@circuit ? @rtepts[1...-1].push(@rtepts[1]) : @rtepts).collect(&:coord)
      coords.each_cons(2) do |coord1, coord2|
        placemark = KML::E.new(:Placemark, :name => "%.2fkm" % coord1.distance_to(coord2), :styleUrl => :rte)
        multi_geometry = KML::E.new(:MultiGeometry)
        multi_geometry << KML::E.new(:Point, :coordinates => coord1.halfway_to(coord2).to_kml)
        multi_geometry << KML::E.new(:LineString, :altitudeMode => :clampToGround, :tessellate => 1, :coordinates => [coord1, coord2].collect(&:to_kml).join(" "))
        bearing = coord2.initial_bearing_to(coord1)
        multi_geometry << KML::E.new(:LineString, :altitudeMode => :clampToGround, :tessellate => 1, :coordinates => [coord2.coord_at(bearing - Math::PI / 12.0, 0.2), coord2, coord2.coord_at(bearing + Math::PI / 12.0, 0.2)].collect(&:to_kml).join(" "))
        placemark << multi_geometry
        folder << placemark
      end
      if @circuit
        placemark = KML::E.new(:Placemark, :styleUrl => :rte2)
        multi_geometry = KML::E.new(:MultiGeometry)
        multi_geometry << KML::E.new(:LineString, :altitudeMode => :clampToGround, :tessellate => 1, :coordinates => [@rtepts[0], @rtepts[1]].collect(&:coord).collect(&:to_kml).join(" "))
        bearing = @rtepts[1].coord.initial_bearing_to(@rtepts[0].coord)
        multi_geometry << KML::E.new(:LineString, :altitudeMode => :clampToGround, :tessellate => 1, :coordinates => [@rtepts[1].coord.coord_at(bearing - Math::PI / 12.0, 0.2), @rtepts[1].coord, @rtepts[1].coord.coord_at(bearing + Math::PI / 12.0, 0.2)].collect(&:to_kml).join(" "))
        placemark << multi_geometry
        folder << placemark
        placemark = KML::E.new(:Placemark, :styleUrl => :rte2)
        multi_geometry = KML::E.new(:MultiGeometry)
        multi_geometry << KML::E.new(:LineString, :altitudeMode => :clampToGround, :tessellate => 1, :coordinates => [@rtepts[-2], @rtepts[-1]].collect(&:coord).collect(&:to_kml).join(" "))
        bearing = @rtepts[-1].coord.initial_bearing_to(@rtepts[-2].coord)
        multi_geometry << KML::E.new(:LineString, :altitudeMode => :clampToGround, :tessellate => 1, :coordinates => [@rtepts[-1].coord.coord_at(bearing - Math::PI / 12.0, 0.2), @rtepts[-1].coord, @rtepts[-1].coord.coord_at(bearing + Math::PI / 12.0, 0.2)].collect(&:to_kml).join(" "))
        placemark << multi_geometry
        folder << placemark
      end
      folder
    end

  end

  class Wpt

    attr_reader :coord, :name

    def initialize(wpt)
      lat = wpt.attributes["lat"]
      lon = wpt.attributes["lon"]
      @coord = Coord.new(lat, lon, 0)
      @name = wpt.elements["name"].text
      @time = wpt.elements["time"].text
    end

    def distance_to(other)
      @coord.distance_to(other.coord)
    end

    def to_kml
      placemark = KML::E.new(:Placemark, :name => @name, :styleUrl => :rte)
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
  document = KML::E.new(:Document)
  document << (KML::E.new(:Style, :attributes => {:id => :track}) << KML::E.new(:LineStyle, :color => "ffff00ff", :width => 2))
  document << (KML::E.new(:Style, :attributes => {:id => :shadow}) << KML::E.new(:LineStyle, :color => "ff000000"))
  style = KML::E.new(:Style, :attributes => {:id => :rte}) 
  style << (KML::E.new(:IconStyle, :color => "ff00ffff", :scale => 0.5) << KML::E.new(:Icon, :href => "http://maps.google.com/mapfiles/kml/pal4/icon24.png"))
  style << KML::E.new(:LabelStyle, :color => "ff00ffff")
  style << KML::E.new(:LineStyle, :color => "ff00ffff")
  document << style
  style = KML::E.new(:Style, :attributes => {:id => :rte2}) 
  style << (KML::E.new(:IconStyle, :color => "8000ffff", :scale => 0.5) << KML::E.new(:Icon, :href => "http://maps.google.com/mapfiles/kml/pal4/icon24.png"))
  style << KML::E.new(:LabelStyle, :color => "8000ffff")
  style << KML::E.new(:LineStyle, :color => "8000ffff")
  document << style
  document << GPX.new($stdin).to_kml
  kml = KML.new(KML::E.new(:kml, document, :attributes => {:xmlns => "http://earth.google.com/kml/2.1"}))
  if !output_filename or output_filename == "-"
    kml.write($stdout)
  else
    File.open(output_filename, "w") { |io| kml.write(io) }
  end
end

exit(main(ARGV) || 0) if $0 == __FILE__
