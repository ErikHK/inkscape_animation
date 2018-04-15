/*
 * Author(s):
 *   Jabiertxo Arraiza Cenoz <jabier.arraiza@marker.es>
 *
 * Copyright (C) 2014 Author(s)
 *
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/lpe-fillet-chamfer.h"

#include "helper/geom.h"
#include "helper/geom-curves.h"
#include "helper/geom-satellite.h"

#include <2geom/elliptical-arc.h>
#include "knotholder.h"
#include "display/curve.h"
#include <boost/optional.hpp>

#include "object/sp-shape.h"

// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

namespace Inkscape {
namespace LivePathEffect {

static const Util::EnumData<Filletmethod> FilletmethodData[] = {
    { FM_AUTO, N_("Auto"), "auto" }, 
    { FM_ARC, N_("Force arc"), "arc" },
    { FM_BEZIER, N_("Force bezier"), "bezier" }
};
static const Util::EnumDataConverter<Filletmethod> FMConverter(FilletmethodData, FM_END);

LPEFilletChamfer::LPEFilletChamfer(LivePathEffectObject *lpeobject)
    : Effect(lpeobject),
      unit(_("Unit"), _("Unit"), "unit", &wr, this, "px"),
      satellites_param("Satellites_param", "Satellites_param",
                       "satellites_param", &wr, this),
      method(_("Method:"), _("Methods to calculate the fillet or chamfer"),
             "method", FMConverter, &wr, this, FM_AUTO),
      mode(_("Mode:"), _("Mode, fillet or chamfer"),
             "mode", &wr, this, "F", true),
      radius(_("Radius (unit or %):"), _("Radius, in unit or %"), "radius", &wr,
             this, 0.0),
      chamfer_steps(_("Chamfer steps:"), _("Chamfer steps"), "chamfer_steps",
                    &wr, this, 1),
      flexible(_("Flexible radius size (%)"), _("Flexible radius size (%)"),
               "flexible", &wr, this, false),
      mirror_knots(_("Mirror Knots"), _("Mirror Knots"), "mirror_knots", &wr,
                   this, true),
      only_selected(_("Change only selected nodes"),
                    _("Change only selected nodes"), "only_selected", &wr, this,
                    false),
      use_knot_distance(_("Use knots distance instead radius"),
                        _("Use knots distance instead radius"),
                        "use_knot_distance", &wr, this, false),
      hide_knots(_("Hide knots"), _("Hide knots"), "hide_knots", &wr, this,
                 false),
      apply_no_radius(_("Apply changes if radius = 0"), _("Apply changes if radius = 0"), "apply_no_radius", &wr, this, true),
      apply_with_radius(_("Apply changes if radius > 0"), _("Apply changes if radius > 0"), "apply_with_radius", &wr, this, true),
      helper_size(_("Helper path size with direction to node:"),
                  _("Helper path size with direction to node"), "helper_size", &wr, this, 0),
      _pathvector_satellites(NULL),
      _degenerate_hide(false)
{
    registerParameter(&satellites_param);
    registerParameter(&unit);
    registerParameter(&method);
    registerParameter(&mode);
    registerParameter(&radius);
    registerParameter(&chamfer_steps);
    registerParameter(&helper_size);
    registerParameter(&flexible);
    registerParameter(&use_knot_distance);
    registerParameter(&mirror_knots);
    registerParameter(&apply_no_radius);
    registerParameter(&apply_with_radius);
    registerParameter(&only_selected);
    registerParameter(&hide_knots);

    radius.param_set_range(0.0, Geom::infinity());
    radius.param_set_increments(1, 1);
    radius.param_set_digits(4);
    radius.param_set_undo(false);
    chamfer_steps.param_set_range(1, 999);
    chamfer_steps.param_set_increments(1, 1);
    chamfer_steps.param_set_digits(0);
    helper_size.param_set_range(0, 999);
    helper_size.param_set_increments(5, 5);
    helper_size.param_set_digits(0);
    _provides_knotholder_entities = true;
}

void LPEFilletChamfer::doOnApply(SPLPEItem const *lpeItem)
{
    SPLPEItem *splpeitem = const_cast<SPLPEItem *>(lpeItem);
    SPShape *shape = dynamic_cast<SPShape *>(splpeitem);
    if (shape) {
        Geom::PathVector const pathv = pathv_to_linear_and_cubic_beziers(shape->getCurve()->get_pathvector());
        Satellites satellites;
        double power = radius;
        if (!flexible) {
            SPDocument * document = SP_ACTIVE_DOCUMENT;
            SPNamedView *nv = sp_document_namedview(document, NULL);
            Glib::ustring display_unit = nv->display_units->abbr;
            power = Inkscape::Util::Quantity::convert(power, unit.get_abbreviation(), display_unit.c_str());
        }
        SatelliteType satellite_type = FILLET;
        std::map<std::string, SatelliteType> gchar_map_to_satellite_type =
        boost::assign::map_list_of("F", FILLET)("IF", INVERSE_FILLET)("C", CHAMFER)("IC", INVERSE_CHAMFER)("KO", INVALID_SATELLITE);
        gchar * mode_str = mode.param_getSVGValue();
        std::map<std::string, SatelliteType>::iterator it = gchar_map_to_satellite_type.find(std::string(mode_str));
        g_free(mode_str);
        if (it != gchar_map_to_satellite_type.end()) {
            satellite_type = it->second;
        }
        for (Geom::PathVector::const_iterator path_it = pathv.begin(); path_it !=  pathv.end(); ++path_it) {
            if (path_it->empty()) {
                continue;
            }
            std::vector<Satellite> subpath_satellites;
            for (Geom::Path::const_iterator curve_it = path_it->begin(); curve_it !=  path_it->end(); ++curve_it) {
                //Maybe we want this satellites...
                //if (curve_it->isDegenerate()) {
                //  continue 
                //}
                Satellite satellite(satellite_type);
                satellite.setSteps(chamfer_steps);
                satellite.setAmount(power);
                satellite.setIsTime(flexible);
                satellite.setHasMirror(mirror_knots);
                satellite.setHidden(hide_knots);
                subpath_satellites.push_back(satellite);
            }
            //we add the last satellite on open path because _pathvector_satellites is related to nodes, not curves
            //so maybe in the future we can need this last satellite in other effects
            //don't remove for this effect because _pathvector_satellites class has methods when the path is modiffied
            //and we want one method for all uses
            if (!path_it->closed()) {
                Satellite satellite(satellite_type);
                satellite.setSteps(chamfer_steps);
                satellite.setAmount(power);
                satellite.setIsTime(flexible);
                satellite.setHasMirror(mirror_knots);
                satellite.setHidden(hide_knots);
                subpath_satellites.push_back(satellite);
            }
            satellites.push_back(subpath_satellites);
        }
        _pathvector_satellites = new PathVectorSatellites();
        _pathvector_satellites->setPathVector(pathv);
        _pathvector_satellites->setSatellites(satellites);
        satellites_param.setPathVectorSatellites(_pathvector_satellites);
    } else {
        g_warning("LPE Fillet/Chamfer can only be applied to shapes (not groups).");
        SPLPEItem *item = const_cast<SPLPEItem *>(lpeItem);
        item->removeCurrentPathEffect(false);
    }
}

Gtk::Widget *LPEFilletChamfer::newWidget()
{
    // use manage here, because after deletion of Effect object, others might
    // still be pointing to this widget.
    Gtk::VBox *vbox = Gtk::manage(new Gtk::VBox(Effect::newWidget()));

    vbox->set_border_width(5);
    vbox->set_homogeneous(false);
    vbox->set_spacing(2);
    std::vector<Parameter *>::iterator it = param_vector.begin();
    while (it != param_vector.end()) {
        if ((*it)->widget_is_visible) {
            Parameter *param = *it;
            Gtk::Widget *widg = param->param_newWidget();
            if (param->param_key == "radius") {
                Inkscape::UI::Widget::Scalar *widg_registered =
                    Gtk::manage(dynamic_cast<Inkscape::UI::Widget::Scalar *>(widg));
                widg_registered->signal_value_changed().connect(
                    sigc::mem_fun(*this, &LPEFilletChamfer::updateAmount));
                widg = widg_registered;
                if (widg) {
                    Gtk::HBox *scalar_parameter = dynamic_cast<Gtk::HBox *>(widg);
                    std::vector<Gtk::Widget *> childList = scalar_parameter->get_children();
                    Gtk::Entry *entry_widget = dynamic_cast<Gtk::Entry *>(childList[1]);
                    entry_widget->set_width_chars(6);
                }
//            } else if (param->param_key == "unit") {
//                Inkscape::UI::Widget::RegisteredUnitMenu* widg_registered = 
//                    Gtk::manage(dynamic_cast< Inkscape::UI::Widget::RegisteredUnitMenu *>(widg));
//                widg_registered->setUnit(unit.get_abbreviation());
//                widg_registered->set_undo_parameters(SP_VERB_DIALOG_LIVE_PATH_EFFECT, _("Change unit parameter"));
//                widg_registered->getUnitMenu()->signal_changed().connect(sigc::mem_fun(*this, &LPEFilletChamfer::convertUnit));
//                widg = widg_registered;
            } else if (param->param_key == "chamfer_steps") {
                Inkscape::UI::Widget::Scalar *widg_registered =
                    Gtk::manage(dynamic_cast<Inkscape::UI::Widget::Scalar *>(widg));
                widg_registered->signal_value_changed().connect(
                    sigc::mem_fun(*this, &LPEFilletChamfer::updateChamferSteps));
                widg = widg_registered;
                if (widg) {
                    Gtk::HBox *scalar_parameter = dynamic_cast<Gtk::HBox *>(widg);
                    std::vector<Gtk::Widget *> childList = scalar_parameter->get_children();
                    Gtk::Entry *entry_widget = dynamic_cast<Gtk::Entry *>(childList[1]);
                    entry_widget->set_width_chars(3);
                }
            } else if (param->param_key == "helper_size") {
                Inkscape::UI::Widget::Scalar *widg_registered =
                    Gtk::manage(dynamic_cast<Inkscape::UI::Widget::Scalar *>(widg));
                widg_registered->signal_value_changed().connect(
                    sigc::mem_fun(*this, &LPEFilletChamfer::refreshKnots));
            } else if (param->param_key == "only_selected") {
                Gtk::manage(widg);
            }
            Glib::ustring *tip = param->param_getTooltip();
            if (widg) {
                vbox->pack_start(*widg, true, true, 2);
                if (tip) {
                    widg->set_tooltip_text(*tip);
                } else {
                    widg->set_tooltip_text("");
                    widg->set_has_tooltip(false);
                }
            }
        }
        ++it;
    }

    Gtk::HBox *fillet_container = Gtk::manage(new Gtk::HBox(true, 0));
    Gtk::Button *fillet =  Gtk::manage(new Gtk::Button(Glib::ustring(_("Fillet"))));
    fillet->signal_clicked()
    .connect(sigc::bind<SatelliteType>(sigc::mem_fun(*this, &LPEFilletChamfer::updateSatelliteType),FILLET));

    fillet_container->pack_start(*fillet, true, true, 2);
    Gtk::Button *inverse_fillet = Gtk::manage(new Gtk::Button(Glib::ustring(_("Inverse fillet"))));
    inverse_fillet->signal_clicked()
    .connect(sigc::bind<SatelliteType>(sigc::mem_fun(*this, &LPEFilletChamfer::updateSatelliteType),INVERSE_FILLET));
    fillet_container->pack_start(*inverse_fillet, true, true, 2);

    Gtk::HBox *chamfer_container = Gtk::manage(new Gtk::HBox(true, 0));
    Gtk::Button *chamfer = Gtk::manage(new Gtk::Button(Glib::ustring(_("Chamfer"))));
    chamfer->signal_clicked()
    .connect(sigc::bind<SatelliteType>(sigc::mem_fun(*this, &LPEFilletChamfer::updateSatelliteType),CHAMFER));

    chamfer_container->pack_start(*chamfer, true, true, 2);
    Gtk::Button *inverse_chamfer = Gtk::manage(new Gtk::Button(Glib::ustring(_("Inverse chamfer"))));
    inverse_chamfer->signal_clicked()
    .connect(sigc::bind<SatelliteType>(sigc::mem_fun(*this, &LPEFilletChamfer::updateSatelliteType),INVERSE_CHAMFER));
    chamfer_container->pack_start(*inverse_chamfer, true, true, 2);

    vbox->pack_start(*fillet_container, true, true, 2);
    vbox->pack_start(*chamfer_container, true, true, 2);
    if(Gtk::Widget* widg = defaultParamSet()) {
        vbox->pack_start(*widg, true, true, 2);
    }
    return vbox;
}

void LPEFilletChamfer::refreshKnots()
{
    if (satellites_param._knoth) {
        satellites_param._knoth->update_knots();
    }
}

void LPEFilletChamfer::updateAmount()
{
    setSelected(_pathvector_satellites);
    double power = radius;
    if (!flexible) {
        SPDocument * document = SP_ACTIVE_DOCUMENT;
        SPNamedView *nv = sp_document_namedview(document, NULL);
        Glib::ustring display_unit = nv->display_units->abbr;
        power = Inkscape::Util::Quantity::convert(power, unit.get_abbreviation(), display_unit.c_str());
    }
    _pathvector_satellites->updateAmount(power, apply_no_radius, apply_with_radius, only_selected, 
                                         use_knot_distance, flexible);
    satellites_param.setPathVectorSatellites(_pathvector_satellites);
}

//void LPEFilletChamfer::convertUnit()
//{
//    SPDocument * document = SP_ACTIVE_DOCUMENT;
//    SPNamedView *nv = sp_document_namedview(document, NULL);
//    Glib::ustring display_unit = nv->display_units->abbr;
//    _pathvector_satellites->convertUnit(unit.get_abbreviation(), display_unit, apply_no_radius, apply_with_radius);
//    satellites_param.setPathVectorSatellites(_pathvector_satellites);
//}

void LPEFilletChamfer::updateChamferSteps()
{
    setSelected(_pathvector_satellites);
    _pathvector_satellites->updateSteps(chamfer_steps, apply_no_radius, apply_with_radius, only_selected);
    satellites_param.setPathVectorSatellites(_pathvector_satellites);
}

void LPEFilletChamfer::updateSatelliteType(SatelliteType satellitetype)
{
    std::map<SatelliteType, gchar const *> satellite_type_to_gchar_map =
    boost::assign::map_list_of(FILLET, "F")(INVERSE_FILLET, "IF")(CHAMFER, "C")(INVERSE_CHAMFER, "IC")(INVALID_SATELLITE, "KO");
    mode.param_setValue((Glib::ustring)satellite_type_to_gchar_map.at(satellitetype));
    setSelected(_pathvector_satellites);
    _pathvector_satellites->updateSatelliteType(satellitetype, apply_no_radius, apply_with_radius, only_selected);
    satellites_param.setPathVectorSatellites(_pathvector_satellites);
}

void LPEFilletChamfer::setSelected(PathVectorSatellites *_pathvector_satellites){
    Geom::PathVector const pathv = _pathvector_satellites->getPathVector();
    Satellites satellites = _pathvector_satellites->getSatellites();
    for (size_t i = 0; i < satellites.size(); ++i) {
        for (size_t j = 0; j < satellites[i].size(); ++j) {
            Geom::Curve const &curve_in = pathv[i][j];
            if (only_selected && isNodePointSelected(curve_in.initialPoint()) ){
                satellites[i][j].setSelected(true);
            } else {
                satellites[i][j].setSelected(false);
            }
        }
    }
    _pathvector_satellites->setSatellites(satellites);
}

void LPEFilletChamfer::doBeforeEffect(SPLPEItem const *lpeItem)
{
    if (!pathvector_before_effect.empty()) {
        //fillet chamfer specific calls
        satellites_param.setUseDistance(use_knot_distance);
        satellites_param.setCurrentZoom(current_zoom);
        //mandatory call
        satellites_param.setEffectType(effectType());
        Geom::PathVector const pathv = pathv_to_linear_and_cubic_beziers(pathvector_before_effect);
        //if are diferent sizes call to recalculate
        //TODO: Update the satellite data in paths modified,
        Satellites satellites = satellites_param.data();
        if (satellites.empty()) {
            doOnApply(lpeItem);
            satellites = satellites_param.data();
        }
        bool write = false;
        if (_pathvector_satellites) {
            size_t number_nodes = pathv.nodes().size();
            size_t previous_number_nodes = _pathvector_satellites->getTotalSatellites();
            if (number_nodes != previous_number_nodes) {
                double power = radius;
                if (!flexible) {
                    SPDocument * document = SP_ACTIVE_DOCUMENT;
                    SPNamedView *nv = sp_document_namedview(document, NULL);
                    Glib::ustring display_unit = nv->display_units->abbr;
                    power = Inkscape::Util::Quantity::convert(power, unit.get_abbreviation(), display_unit.c_str());
                }
                SatelliteType satellite_type = FILLET;
                std::map<std::string, SatelliteType> gchar_map_to_satellite_type =
                boost::assign::map_list_of("F", FILLET)("IF", INVERSE_FILLET)("C", CHAMFER)("IC", INVERSE_CHAMFER)("KO", INVALID_SATELLITE);
                gchar * mode_str = mode.param_getSVGValue();
                std::map<std::string, SatelliteType>::iterator it = gchar_map_to_satellite_type.find(std::string(mode_str));
                g_free(mode_str);
                if (it != gchar_map_to_satellite_type.end()) {
                    satellite_type = it->second;
                }
                Satellite satellite(satellite_type);
                satellite.setSteps(chamfer_steps);
                satellite.setAmount(power);
                satellite.setIsTime(flexible);
                satellite.setHasMirror(mirror_knots);
                satellite.setHidden(hide_knots);
                _pathvector_satellites->recalculateForNewPathVector(pathv, satellite);
                satellites = _pathvector_satellites->getSatellites();
                write = true;
            }
        }
        if (_degenerate_hide) {
            satellites_param.setGlobalKnotHide(true);
        } else {
            satellites_param.setGlobalKnotHide(false);
        }
        if (hide_knots) {
            satellites_param.setHelperSize(0);
        } else {
            satellites_param.setHelperSize(helper_size);
        }
        for (size_t i = 0; i < satellites.size(); ++i) {
            for (size_t j = 0; j < satellites[i].size(); ++j) {
                if (j >= pathv[i].size()) {
                    continue;
                }
                Geom::Curve const &curve_in = pathv[i][j];
                if (satellites[i][j].is_time != flexible) {
                    satellites[i][j].is_time = flexible;
                    double amount = satellites[i][j].amount;
                    if (satellites[i][j].is_time) {
                        double time = timeAtArcLength(amount, curve_in);
                        satellites[i][j].amount = time;
                    } else {
                        double size = arcLengthAt(amount, curve_in);
                        satellites[i][j].amount = size;
                    }
                }
                if (satellites[i][j].has_mirror != mirror_knots) {
                    satellites[i][j].has_mirror = mirror_knots;
                }
                satellites[i][j].hidden = hide_knots;
                if (only_selected && isNodePointSelected(curve_in.initialPoint()) ){
                    satellites[i][j].setSelected(true);
                }
            }
        }
        if (!_pathvector_satellites) {
            _pathvector_satellites = new PathVectorSatellites();
        }
        _pathvector_satellites->setPathVector(pathv);
        _pathvector_satellites->setSatellites(satellites);
        satellites_param.setPathVectorSatellites(_pathvector_satellites, write);
        refreshKnots();
    } else {
        g_warning("LPE Fillet can only be applied to shapes (not groups).");
    }
}

void
LPEFilletChamfer::addCanvasIndicators(SPLPEItem const */*lpeitem*/, std::vector<Geom::PathVector> &hp_vec)
{
    hp_vec.push_back(_hp);
}

void
LPEFilletChamfer::addChamferSteps(Geom::Path &tmp_path, Geom::Path path_chamfer, Geom::Point end_arc_point, size_t steps)
{
    setSelected(_pathvector_satellites);
    double path_subdivision = 1.0 / steps;
    for (size_t i = 1; i < steps; i++) {
        Geom::Point chamfer_step = path_chamfer.pointAt(path_subdivision * i);
        tmp_path.appendNew<Geom::LineSegment>(chamfer_step);
    }
    tmp_path.appendNew<Geom::LineSegment>(end_arc_point);
}

Geom::PathVector
LPEFilletChamfer::doEffect_path(Geom::PathVector const &path_in)
{
    const double GAP_HELPER = 0.00001;
    Geom::PathVector path_out;
    size_t path = 0;
    const double K = (4.0 / 3.0) * (sqrt(2.0) - 1.0);
    _degenerate_hide = false;
    Geom::PathVector const pathv = pathv_to_linear_and_cubic_beziers(path_in);
    Satellites satellites = _pathvector_satellites->getSatellites();
    for (Geom::PathVector::const_iterator path_it = pathv.begin(); path_it != pathv.end(); ++path_it) {
        if (path_it->empty()) {
            continue;
        }
        Geom::Path tmp_path;
        if (path_it->size() == 1) {
            path++;
            tmp_path.start(path_it[0].pointAt(0));
            tmp_path.append(path_it[0]);
            path_out.push_back(tmp_path);
            continue;
        }
        double time0 = 0;
        size_t curve = 0;
        for (Geom::Path::const_iterator curve_it1 = path_it->begin(); curve_it1 !=  path_it->end(); ++curve_it1) {
            size_t next_index = curve + 1;
            if (curve == pathv[path].size() - 1 && pathv[path].closed()) {
                next_index = 0;
            }
            //append last extreme of paths on open paths
            if (curve == pathv[path].size() -1 && !pathv[path].closed()) { //the path is open and we are at end of path
                if (time0 != 1) { //Previous satellite not at 100% amount
                    Geom::Curve *last_curve = curve_it1->portion(time0, 1);
                    last_curve->setInitial(tmp_path.finalPoint());
                    tmp_path.append(*last_curve);
                }
                continue;
            }
            Geom::Curve const &curve_it2 = pathv[path][next_index];
            Satellite satellite = satellites[path][next_index];
            if (Geom::are_near((*curve_it1).initialPoint(), (*curve_it1).finalPoint())) {
                _degenerate_hide = true;
                g_warning("Knots hidden if consecutive nodes has the same position.");
                return path_in;
            }
            if (!curve) { //curve == 0 
                if (!path_it->closed()) {
                    time0 = 0;
                } else {
                    time0 = satellites[path][0].time(*curve_it1);
                }
            }
            double s = satellite.arcDistance(curve_it2);
            double time1 = satellite.time(s, true, (*curve_it1));
            double time2 = satellite.time(curve_it2);
            if (time1 <= time0) {
                time1 = time0;
            }
            if (time2 > 1) {
                time2 = 1;
            }
            Geom::Curve *knot_curve_1 = curve_it1->portion(time0, time1);
            Geom::Curve *knot_curve_2 = curve_it2.portion(time2, 1);
            if (curve > 0) {
                knot_curve_1->setInitial(tmp_path.finalPoint());
            } else {
                tmp_path.start((*curve_it1).pointAt(time0));
            }

            Geom::Point start_arc_point = knot_curve_1->finalPoint();
            Geom::Point end_arc_point = curve_it2.pointAt(time2);
            //add a gap helper
            if (time2 == 1) {
                end_arc_point = curve_it2.pointAt(time2 - GAP_HELPER);
            }
            if (time1 == time0) {
                start_arc_point = curve_it1->pointAt(time1 + GAP_HELPER);
            }

            double k1 = distance(start_arc_point, curve_it1->finalPoint()) * K;
            double k2 = distance(curve_it2.initialPoint(), end_arc_point) * K;
            Geom::CubicBezier const *cubic_1 = dynamic_cast<Geom::CubicBezier const *>(&*knot_curve_1);
            Geom::CubicBezier const *cubic_2 = dynamic_cast<Geom::CubicBezier const *>(&*knot_curve_2);
            Geom::Ray ray_1(start_arc_point, curve_it1->finalPoint());
            Geom::Ray ray_2(curve_it2.initialPoint(), end_arc_point);
            if (cubic_1) {
                ray_1.setPoints((*cubic_1)[2], start_arc_point);
            }
            if (cubic_2) {
                ray_2.setPoints(end_arc_point, (*cubic_2)[1]);
            }
            bool ccw_toggle = cross(curve_it1->finalPoint() - start_arc_point, end_arc_point - start_arc_point) < 0;
            double angle = angle_between(ray_1, ray_2, ccw_toggle);
            double handle_angle_1 = ray_1.angle() - angle;
            double handle_angle_2 = ray_2.angle() + angle;
            if (ccw_toggle) {
                handle_angle_1 = ray_1.angle() + angle;
                handle_angle_2 = ray_2.angle() - angle;
            }
            Geom::Point handle_1 = Geom::Point::polar(ray_1.angle(), k1) + start_arc_point;
            Geom::Point handle_2 = end_arc_point - Geom::Point::polar(ray_2.angle(), k2);
            Geom::Point inverse_handle_1 = Geom::Point::polar(handle_angle_1, k1) + start_arc_point;
            Geom::Point inverse_handle_2 = end_arc_point - Geom::Point::polar(handle_angle_2, k2);
            if (time0 == 1) {
                handle_1 = start_arc_point;
                inverse_handle_1 = start_arc_point;
            }
            //remove gap helper
            if (time2 == 1) {
                end_arc_point = curve_it2.pointAt(time2);
            }
            if (time1 == time0) {
                start_arc_point = curve_it1->pointAt(time0);
            }
            if (time1 != 1 && !Geom::are_near(angle,Geom::rad_from_deg(360))) {
                if (time1 != time0 || (time1 == 1 && time0 == 1)) {
                    if (!knot_curve_1->isDegenerate()) {
                        tmp_path.append(*knot_curve_1);
                    }
                }
                SatelliteType type = satellite.satellite_type;
                size_t steps = satellite.steps;
                if (!steps) steps = 1;
                Geom::Line const x_line(Geom::Point(0, 0), Geom::Point(1, 0));
                Geom::Line const angled_line(start_arc_point, end_arc_point);
                double arc_angle = Geom::angle_between(x_line, angled_line);
                double radius = Geom::distance(start_arc_point, middle_point(start_arc_point, end_arc_point)) /
                                               sin(angle / 2.0);
                Geom::Coord rx = radius;
                Geom::Coord ry = rx;
                bool eliptical = (is_straight_curve(*curve_it1) &&
                                  is_straight_curve(curve_it2) && method != FM_BEZIER) ||
                                  method == FM_ARC;
                switch (type) {
                case CHAMFER:
                    {
                        Geom::Path path_chamfer;
                        path_chamfer.start(tmp_path.finalPoint());
                        if (eliptical) {
                            ccw_toggle = ccw_toggle ? 0 : 1;
                            path_chamfer.appendNew<Geom::EllipticalArc>(rx, ry, arc_angle, 0, ccw_toggle, end_arc_point);
                        } else {
                            path_chamfer.appendNew<Geom::CubicBezier>(handle_1, handle_2, end_arc_point);
                        }
                        addChamferSteps(tmp_path, path_chamfer, end_arc_point, steps);
                    }
                    break;
                case INVERSE_CHAMFER:
                    {
                        Geom::Path path_chamfer;
                        path_chamfer.start(tmp_path.finalPoint());
                        if (eliptical) {
                            path_chamfer.appendNew<Geom::EllipticalArc>(rx, ry, arc_angle, 0, ccw_toggle, end_arc_point);
                        } else {
                            path_chamfer.appendNew<Geom::CubicBezier>(inverse_handle_1, inverse_handle_2, end_arc_point);
                        }
                        addChamferSteps(tmp_path, path_chamfer, end_arc_point, steps);
                    }
                    break;
                case INVERSE_FILLET:
                    {
                        if (eliptical) {
                            tmp_path.appendNew<Geom::EllipticalArc>(rx, ry, arc_angle, 0, ccw_toggle, end_arc_point);
                        } else {
                            tmp_path.appendNew<Geom::CubicBezier>(inverse_handle_1, inverse_handle_2, end_arc_point);
                        }
                    }
                    break;
                default: //fillet
                    {
                        if (eliptical) {
                            ccw_toggle = ccw_toggle ? 0 : 1;
                            tmp_path.appendNew<Geom::EllipticalArc>(rx, ry, arc_angle, 0, ccw_toggle, end_arc_point);
                        } else {
                            tmp_path.appendNew<Geom::CubicBezier>(handle_1, handle_2, end_arc_point);
                        }
                    }
                    break;
                }
            } else {
                if (!knot_curve_1->isDegenerate()) {
                    tmp_path.append(*knot_curve_1);
                }
            }
            curve++;
            time0 = time2;
        }
        if (path_it->closed()) {
            tmp_path.close();
        }
        path++;
        path_out.push_back(tmp_path);
    }
    return path_out;
}

}; //namespace LivePathEffect
}; /* namespace Inkscape */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offset:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
