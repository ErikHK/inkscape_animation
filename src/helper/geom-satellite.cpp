/**
 * \file
 * \brief Satellite a per node holder of data.
 */ /*
    * Authors:
    * 2015 Jabier Arraiza Cenoz<jabier.arraiza@marker.es>
    *
    * This code is in public domain
    */

#include <helper/geom-satellite.h>
#include <2geom/curve.h>
#include <2geom/nearest-time.h>
#include <2geom/path-intersection.h>
#include <2geom/sbasis-to-bezier.h>
#include <2geom/ray.h>
#include <boost/optional.hpp>
//log cache
#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#include <ctime>
#endif

///@brief Satellite a per node holder of data.
Satellite::Satellite() {}


Satellite::Satellite(SatelliteType satellite_type)
    : satellite_type(satellite_type),
      is_time(false),
      selected(false),
      has_mirror(false),
      hidden(true),
      amount(0.0),
      angle(0.0),
      steps(0)
{}

Satellite::~Satellite() {}

///Calculate the time in curve_in with a size of A
//TODO: find a better place to it
double timeAtArcLength(double const A, Geom::Curve const &curve_in)
{
    if ( A == 0 || curve_in.isDegenerate()) {
        return 0;
    }

    Geom::D2<Geom::SBasis> d2_in = curve_in.toSBasis();
    double t = 0;
    double length_part = curve_in.length();
    if (A >= length_part || curve_in.isLineSegment()) {
        if (length_part != 0) {
            t = A / length_part;
        }
    } else if (!curve_in.isLineSegment()) {
        std::vector<double> t_roots = roots(Geom::arcLengthSb(d2_in) - A);
        if (!t_roots.empty()) {
            t = t_roots[0];
        }
    }
    return t;
}

///Calculate the size in curve_in with a point at A
//TODO: find a better place to it
double arcLengthAt(double const A, Geom::Curve const &curve_in)
{
    if ( A == 0 || curve_in.isDegenerate()) {
        return 0;
    }

    double s = 0;
    double length_part = curve_in.length();
    if (A > length_part || curve_in.isLineSegment()) {
        s = (A * length_part);
    } else if (!curve_in.isLineSegment()) {
        Geom::Curve *curve = curve_in.portion(0.0, A);
        s = curve->length();
        delete curve;
    }
    return s;
}

///Convert a arc radius of a fillet/chamfer to his satellite length -point position where fillet/chamfer knot be on original curve
double Satellite::radToLen(
    double const A, Geom::Curve const &curve_in,
    Geom::Curve const &curve_out) const
{
    double len = 0;
    Geom::D2<Geom::SBasis> d2_in = curve_in.toSBasis();
    Geom::D2<Geom::SBasis> d2_out = curve_out.toSBasis();
    Geom::Piecewise<Geom::D2<Geom::SBasis> > offset_curve0 = 
        Geom::Piecewise<Geom::D2<Geom::SBasis> >(d2_in) +
        rot90(unitVector(derivative(d2_in))) * (A);
    Geom::Piecewise<Geom::D2<Geom::SBasis> > offset_curve1 =
        Geom::Piecewise<Geom::D2<Geom::SBasis> >(d2_out) +
        rot90(unitVector(derivative(d2_out))) * (A);
    Geom::Path p0 = path_from_piecewise(offset_curve0, 0.1)[0];
    Geom::Path p1 = path_from_piecewise(offset_curve1, 0.1)[0];
    Geom::Crossings cs = Geom::crossings(p0, p1);
    if (cs.size() > 0) {
        Geom::Point cp = p0(cs[0].ta);
        double p0pt = nearest_time(cp, curve_out);
        len = arcLengthAt(p0pt, curve_out);
    } else {
        if (A > 0) {
            len = radToLen(A * -1, curve_in, curve_out);
        }
    }
    return len;
}

///Convert a satellite length -point position where fillet/chamfer knot be on original curve- to a arc radius of fillet/chamfer
double Satellite::lenToRad(
    double const A, Geom::Curve const &curve_in,
    Geom::Curve const &curve_out,
    Satellite const previousSatellite) const
{
    double time_in = (previousSatellite).time(A, true, curve_in);
    double time_out = timeAtArcLength(A, curve_out);
    Geom::Point start_arc_point = curve_in.pointAt(time_in);
    Geom::Point end_arc_point = curve_out.pointAt(time_out);
    Geom::Curve *knot_curve1 = curve_in.portion(0, time_in);
    Geom::Curve *knot_curve2 = curve_out.portion(time_out, 1);
    Geom::CubicBezier const *cubic1 = dynamic_cast<Geom::CubicBezier const *>(&*knot_curve1);
    Geom::Ray ray1(start_arc_point, curve_in.pointAt(1));
    if (cubic1) {
        ray1.setPoints((*cubic1)[2], start_arc_point);
    }
    Geom::CubicBezier const *cubic2 = dynamic_cast<Geom::CubicBezier const *>(&*knot_curve2);
    Geom::Ray ray2(curve_out.pointAt(0), end_arc_point);
    if (cubic2) {
        ray2.setPoints(end_arc_point, (*cubic2)[1]);
    }
    bool ccw_toggle = cross(curve_in.pointAt(1) - start_arc_point,
                           end_arc_point - start_arc_point) < 0;
    double distance_arc =
        Geom::distance(start_arc_point, middle_point(start_arc_point, end_arc_point));
    double angle = angle_between(ray1, ray2, ccw_toggle);
    double divisor = std::sin(angle / 2.0);
    if (divisor > 0) {
        return distance_arc / divisor;
    }
    return 0;
}

///Get the time position of the satellite in curve_in
double Satellite::time(Geom::Curve const &curve_in, bool inverse) const
{
    double t = amount;
    if (!is_time) {
        t = time(t, inverse, curve_in);
    } else if (inverse) {
        t = 1-t;
    }
    if (t > 1) {
        t = 1;
    }
    return t;
}

///Get the time from a length A in other curve, a boolean inverse given to reverse time
double Satellite::time(double A, bool inverse,
                       Geom::Curve const &curve_in) const
{
    if (A == 0 && inverse) {
        return 1;
    }
    if (A == 0 && !inverse) {
        return 0;
    }
    if (!inverse) {
        return timeAtArcLength(A, curve_in);
    }
    double length_part = curve_in.length();
    A = length_part - A;
    return timeAtArcLength(A, curve_in);
}

///Get the length of the satellite in curve_in
double Satellite::arcDistance(Geom::Curve const &curve_in) const
{
    double s = amount;
    if (is_time) {
        s = arcLengthAt(s, curve_in);
    }
    return s;
}

///Get the point position of the satellite
Geom::Point Satellite::getPosition(Geom::Curve const &curve_in, bool inverse) const
{
    double t = time(curve_in, inverse);
    return curve_in.pointAt(t);
}

///Set the position of the satellite from a given point P
void Satellite::setPosition(Geom::Point const p, Geom::Curve const &curve_in, bool inverse)
{
    Geom::Curve * curve = const_cast<Geom::Curve *>(&curve_in);
    if (inverse) {
        curve = curve->reverse();
    }
    double A = Geom::nearest_time(p, *curve);
    if (!is_time) {
        A = arcLengthAt(A, *curve);
    }
    amount = A;
}


///Map a satellite type with gchar
void Satellite::setSatelliteType(gchar const *A)
{
    std::map<std::string, SatelliteType> gchar_map_to_satellite_type =
        boost::assign::map_list_of("F", FILLET)("IF", INVERSE_FILLET)("C", CHAMFER)("IC", INVERSE_CHAMFER)("KO", INVALID_SATELLITE);
    std::map<std::string, SatelliteType>::iterator it = gchar_map_to_satellite_type.find(std::string(A));
    if (it != gchar_map_to_satellite_type.end()) {
        satellite_type = it->second;
    }
}

///Map a gchar with satelliteType
gchar const *Satellite::getSatelliteTypeGchar() const
{
    std::map<SatelliteType, gchar const *> satellite_type_to_gchar_map =
        boost::assign::map_list_of(FILLET, "F")(INVERSE_FILLET, "IF")(CHAMFER, "C")(INVERSE_CHAMFER, "IC")(INVALID_SATELLITE, "KO");
    return satellite_type_to_gchar_map.at(satellite_type);
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim:
// filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99
// :
