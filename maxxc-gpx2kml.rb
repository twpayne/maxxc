#!/usr/bin/ruby

require "optparse"
require "rexml/document"

class Symbol

  def to_proc
    lambda { |object| object.send(self) }
  end

end

class Coord

  attr_reader :lat, :lon, :ele

  def initialize(lat, lon, ele)
    @lat, @lon, @ele = lat, lon, ele
  end

  def to_kml
    "%f,%f,%f" % [@lon, @lat, @ele]
  end

end

class GPX

  class Rte

    attr_reader :score

    def initialize(element)
      @name = element.elements["//name"].text
      @distance = element.elements["//distance"].text
      @score = element.elements["//score"].text
      @multiplier = element.elements["//multiplier"].text
    end

    def to_kml
      placemark = KML::ComplexElement.new(:Placemark)
      placemark << KML::SimpleElement.new(:name, @name)
      placemark << KML::SimpleElement.new(:description,
        "<![CDATA[" +
        "<table>" +
        "<tr><td>Distance</td><td>#{@distance}km</td></tr>" +
        "<tr><td>Multiplier</td><td>\xc3\x97 #{@multiplier} points/km</td></tr>" +
        "<tr><td>Score</td><td>#{@score} points</td></tr>" +
        "</table>" +
        "]]>")
      placemark
    end

  end

  def initialize(io)
    document = REXML::Document.new(io)
    @league = document.elements["gpx/metadata/extensions/league"].text
    @rtes = []
    document.elements.each("gpx/rte") do |element|
      @rtes << Rte.new(element)
    end
    @rtes.sort_by { |rte| rte.score.to_f }
  end

  def to_kml
    folder = KML::ComplexElement.new(:Folder)
    folder << KML::SimpleElement.new(:name, @league)
    @rtes.each do |rte|
      folder << rte.to_kml
    end
    folder
  end

end

module KML

  class Element

    def initialize(tag)
      @tag = tag
    end

  end

  class SimpleElement < Element

    def initialize(tag, text)
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

  class ComplexElement < Element

    def initialize(tag, *args)
      super(tag)
      @attributes, @children = {}, []
      args.each do |arg|
        case arg
        when Hash then @attributes.merge!(arg)
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
      @attributes.each { |key, value| io.write(" #{key}=\"#{value}\"") }
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
    folder = KML::ComplexElement.new(:Folder)
    folder << KML::SimpleElement.new(:name, @name)
    tracklog = KML::ComplexElement.new(:Placemark)
    tracklog << KML::SimpleElement.new(:name, "Tracklog")
    line_style = KML::ComplexElement.new(:LineStyle)
    line_style << KML::SimpleElement.new(:color, "ff0000ff")
    line_style << KML::SimpleElement.new(:width, 2)
    tracklog << KML::ComplexElement.new(:Style, line_style)
    line_string = KML::ComplexElement.new(:LineString)
    line_string << KML::SimpleElement.new(:altitudeMode, :absolute)
    coordinates = KML::SimpleElement.new(:coordinates, @coords.collect(&:to_kml).join(" "))
    line_string << coordinates
    tracklog << line_string
    folder << tracklog
    shadow = KML::ComplexElement.new(:Placemark)
    shadow << KML::SimpleElement.new(:name, "Shadow")
    line_style = KML::ComplexElement.new(:LineStyle)
    line_style << KML::SimpleElement.new(:color, "ff000000")
    shadow << KML::ComplexElement.new(:Style, line_style)
    line_string = KML::ComplexElement.new(:LineString)
    line_string << KML::SimpleElement.new(:altitudeMode, :clampToGround)
    line_string << coordinates
    shadow << line_string
    folder << shadow
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
  document = KML::ComplexElement.new(:Document)
  document << GPX.new($stdin).to_kml
  argv.each do |arg|
    case arg
    when /\.igc\z/i
      File.open(arg) do |io|
        document << IGC.new(io).to_kml
      end
    end
  end
  kml = KML::ComplexElement.new(:kml, :xmlns => "http://earth.google.com/kml/2.1")
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
