/*
 * Author(s):
 *   Jabiertxo Arraiza Cenoz <jabier.arraiza@marker.es>
 *
 * Copyright (C) 2014 Author(s)
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "knotholder.h"
#include "ui/dialog/lpe-fillet-chamfer-properties.h"
#include "live_effects/parameter/satellitesarray.h"
#include "live_effects/effect.h"

#include "inkscape.h"
#include <preferences.h>
// TODO due to internal breakage in glibmm headers,
// this has to be included last.
#include <glibmm/i18n.h>

namespace Inkscape {

namespace LivePathEffect {

SatellitesArrayParam::SatellitesArrayParam(const Glib::ustring &label,
        const Glib::ustring &tip,
        const Glib::ustring &key,
        Inkscape::UI::Widget::Registry *wr,
        Effect *effect)
    : ArrayParam<std::vector<Satellite> >(label, tip, key, wr, effect, 0), _knoth(NULL)
{
    _knot_shape = SP_KNOT_SHAPE_DIAMOND;
    _knot_mode = SP_KNOT_MODE_XOR;
    _knot_color = 0xAAFF8800;
    _helper_size = 0;
    _use_distance = false;
    _global_knot_hide = false;
    _current_zoom = 0;
    _effectType = FILLET_CHAMFER;
    _last_pathvector_satellites = NULL;
    param_widget_is_visible(false);
}


void SatellitesArrayParam::set_oncanvas_looks(SPKnotShapeType shape,
        SPKnotModeType mode,
        guint32 color)
{
    _knot_shape = shape;
    _knot_mode = mode;
    _knot_color = color;
}

void SatellitesArrayParam::setPathVectorSatellites(PathVectorSatellites *pathVectorSatellites, bool write)
{
    _last_pathvector_satellites = pathVectorSatellites;
    if (write) {
        param_set_and_write_new_value(_last_pathvector_satellites->getSatellites());
    } else {
        param_setValue(_last_pathvector_satellites->getSatellites());
    }
}

void SatellitesArrayParam::setUseDistance(bool use_knot_distance)
{
    _use_distance = use_knot_distance;
}

void SatellitesArrayParam::setCurrentZoom(double current_zoom)
{
    _current_zoom = current_zoom;
}

void SatellitesArrayParam::setGlobalKnotHide(bool global_knot_hide)
{
    _global_knot_hide = global_knot_hide;
}
void SatellitesArrayParam::setEffectType(EffectType et)
{
    _effectType = et;
}

void SatellitesArrayParam::setHelperSize(int hs)
{
    _helper_size = hs;
    updateCanvasIndicators();
}

void SatellitesArrayParam::updateCanvasIndicators(bool mirror)
{
    if (!_last_pathvector_satellites) {
        return;
    }

    if (!_hp.empty()) {
        _hp.clear();
    }
    Geom::PathVector pathv = _last_pathvector_satellites->getPathVector();
    if (pathv.empty()) {
        return;
    }
    if (mirror == true) {
        _hp.clear();
    }
    if (_effectType == FILLET_CHAMFER) {
        for (size_t i = 0; i < _vector.size(); ++i) {
            for (size_t j = 0; j < _vector[i].size(); ++j) {
                if (_vector[i][j].hidden || //Ignore if hidden
                    (!_vector[i][j].has_mirror && mirror == true) || //Ignore if not have mirror and we are in mirror loop
                    _vector[i][j].amount == 0 || //no helper in 0 value
                    j >= pathv[i].size() || //ignore last satellite in open paths with fillet chamfer effect
                    (!pathv[i].closed() && j == 0) ||  //ignore first satellites on open paths
                    pathv[i].size() == 2)
                {
                    continue;
                }
                Geom::Curve *curve_in = pathv[i][j].duplicate();
                double pos = 0;
                bool overflow = false;
                double size_out = _vector[i][j].arcDistance(*curve_in);
                double lenght_out = curve_in->length();
                gint previous_index = j - 1; //Always are previous index because we skip first satellite on open paths
                if (j == 0 && pathv[i].closed()) {
                    previous_index = pathv[i].size() - 1;
                }
                if ( previous_index < 0 ) {
                    return;
                }
                double lenght_in = pathv.curveAt(previous_index).length();
                if (mirror) {
                    curve_in = const_cast<Geom::Curve *>(&pathv.curveAt(previous_index));
                    pos = _vector[i][j].time(size_out, true, *curve_in);
                    if (lenght_out < size_out) {
                        overflow = true;
                    }
                } else {
                    pos = _vector[i][j].time(*curve_in);
                    if (lenght_in < size_out) {
                        overflow = true;
                    }
                }
                if (pos <= 0 || pos >= 1) {
                    continue;
                }
                Geom::Point point_a = curve_in->pointAt(pos);
                Geom::Point deriv_a = unit_vector(derivative(curve_in->toSBasis()).pointAt(pos));
                Geom::Rotate rot(Geom::Rotate::from_degrees(-90));
                deriv_a = deriv_a * rot;
                Geom::Point point_c = point_a - deriv_a * _helper_size;
                Geom::Point point_d = point_a + deriv_a * _helper_size;
                Geom::Ray ray_1(point_c, point_d);
                char const *svgd = "M 1,0.25 0.5,0 1,-0.25 M 1,0.5 0,0 1,-0.5";
                Geom::PathVector pathv = sp_svg_read_pathv(svgd);
                Geom::Affine aff = Geom::Affine();
                aff *= Geom::Scale(_helper_size);
                if (mirror) {
                    aff *= Geom::Rotate(ray_1.angle() - Geom::rad_from_deg(90));
                } else {
                    aff *= Geom::Rotate(ray_1.angle() - Geom::rad_from_deg(270));
                }
                aff *= Geom::Translate(curve_in->pointAt(pos));
                pathv *= aff;
                _hp.push_back(pathv[0]);
                _hp.push_back(pathv[1]);
                if (overflow) {
                    double diameter = _helper_size;
                    if (_helper_size == 0) {
                        diameter = 15;
                        char const *svgd;
                        svgd = "M 0.7,0.35 A 0.35,0.35 0 0 1 0.35,0.7 0.35,0.35 0 0 1 0,0.35 "
                               "0.35,0.35 0 0 1 0.35,0 0.35,0.35 0 0 1 0.7,0.35 Z";
                        Geom::PathVector pathv = sp_svg_read_pathv(svgd);
                        aff = Geom::Affine();
                        aff *= Geom::Scale(diameter);
                        aff *= Geom::Translate(point_a - Geom::Point(diameter * 0.35, diameter * 0.35));
                        pathv *= aff;
                        _hp.push_back(pathv[0]);
                    } else {
                        char const *svgd;
                        svgd = "M 0 -1.32 A 1.32 1.32 0 0 0 -1.32 0 A 1.32 1.32 0 0 0 0 1.32 A "
                               "1.32 1.32 0 0 0 1.18 0.59 L 0 0 L 1.18 -0.59 A 1.32 1.32 0 0 0 "
                               "0 -1.32 z";
                        Geom::PathVector pathv = sp_svg_read_pathv(svgd);
                        aff = Geom::Affine();
                        aff *= Geom::Scale(_helper_size / 2.0);
                        if (mirror) {
                            aff *= Geom::Rotate(ray_1.angle() - Geom::rad_from_deg(90));
                        } else {
                            aff *= Geom::Rotate(ray_1.angle() - Geom::rad_from_deg(270));
                        }
                        aff *= Geom::Translate(curve_in->pointAt(pos));
                        pathv *= aff;
                        _hp.push_back(pathv[0]);
                    }
                }
            }
        }
    }
    if (!_knot_reset_helper.empty()) {
        _hp.insert(_hp.end(), _knot_reset_helper.begin(), _knot_reset_helper.end() );
    }
    if (mirror) {
        updateCanvasIndicators(false);
    }
}
void SatellitesArrayParam::updateCanvasIndicators()
{
    updateCanvasIndicators(true);
}

void SatellitesArrayParam::addCanvasIndicators(
    SPLPEItem const */*lpeitem*/, std::vector<Geom::PathVector> &hp_vec)
{
    hp_vec.push_back(_hp);
}

void SatellitesArrayParam::param_transform_multiply(Geom::Affine const &postmul, bool /*set*/)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    if (prefs->getBool("/options/transform/rectcorners", true)) {
        for (size_t i = 0; i < _vector.size(); ++i) {
            for (size_t j = 0; j < _vector[i].size(); ++j) {
                if (!_vector[i][j].is_time && _vector[i][j].amount > 0) {
                    _vector[i][j].amount = _vector[i][j].amount * ((postmul.expansionX() + postmul.expansionY()) / 2);
                }
            }
        }
        param_set_and_write_new_value(_vector);
    }
}

void SatellitesArrayParam::addKnotHolderEntities(KnotHolder *knotholder,
                                                 SPItem *item,
                                                 bool mirror)
{
    if (!_last_pathvector_satellites) {
        return;
    }
    Geom::PathVector pathv = _last_pathvector_satellites->getPathVector();
    size_t index = 0;
    for (size_t i = 0; i < _vector.size(); ++i) {
        for (size_t j = 0; j < _vector[i].size(); ++j) {
            if (!_vector[i][j].has_mirror && mirror) {
                continue;
            }
            SatelliteType type = _vector[i][j].satellite_type;
            if (mirror && i == 0 && j == 0) {
                index = index + _last_pathvector_satellites->getTotalSatellites();
            }
            using namespace Geom;
            //If is for filletChamfer effect...
            if (_effectType == FILLET_CHAMFER) {
                const gchar *tip;
                if (type == CHAMFER) {
                    tip = _("<b>Chamfer</b>: <b>Ctrl+Click</b> toggles type, "
                            "<b>Shift+Click</b> open dialog, "
                            "<b>Ctrl+Alt+Click</b> reset");
                } else if (type == INVERSE_CHAMFER) {
                    tip = _("<b>Inverse Chamfer</b>: <b>Ctrl+Click</b> toggles type, "
                            "<b>Shift+Click</b> open dialog, "
                            "<b>Ctrl+Alt+Click</b> reset");
                } else if (type == INVERSE_FILLET) {
                    tip = _("<b>Inverse Fillet</b>: <b>Ctrl+Click</b> toggles type, "
                            "<b>Shift+Click</b> open dialog, "
                            "<b>Ctrl+Alt+Click</b> reset");
                } else {
                    tip = _("<b>Fillet</b>: <b>Ctrl+Click</b> toggles type, "
                            "<b>Shift+Click</b> open dialog, "
                            "<b>Ctrl+Alt+Click</b> reset");
                }
                FilletChamferKnotHolderEntity *e = new FilletChamferKnotHolderEntity(this, index);
                e->create(NULL, item, knotholder, Inkscape::CTRL_TYPE_UNKNOWN, _(tip),_knot_shape, _knot_mode, _knot_color);
                knotholder->add(e);
            }
            index++;
        }
    }
    if (mirror) {
        addKnotHolderEntities(knotholder, item, false);
    }
}

void SatellitesArrayParam::addKnotHolderEntities(KnotHolder *knotholder,
        SPItem *item)
{
    _knoth = knotholder;
    addKnotHolderEntities(knotholder, item, true);
}

FilletChamferKnotHolderEntity::FilletChamferKnotHolderEntity(
    SatellitesArrayParam *p, size_t index)
    : _pparam(p), _index(index) {}

void FilletChamferKnotHolderEntity::knot_set(Geom::Point const &p,
        Geom::Point const &/*origin*/,
        guint state)
{
    if (!_pparam->_last_pathvector_satellites) {
        return;
    }
    size_t total_satellites = _pparam->_last_pathvector_satellites->getTotalSatellites();
    bool is_mirror = false;
    size_t index = _index;
    if (_index >= total_satellites) {
        index = _index - total_satellites;
        is_mirror = true;
    }
    std::pair<size_t, size_t> index_data = _pparam->_last_pathvector_satellites->getIndexData(index);
    size_t path_index = index_data.first;
    size_t curve_index = index_data.second;
    
    Geom::Point s = snap_knot_position(p, state);
    if (!valid_index(path_index, curve_index)) {
        return;
    }
    Satellite satellite = _pparam->_vector[path_index][curve_index];
    Geom::PathVector pathv = _pparam->_last_pathvector_satellites->getPathVector();
    if (satellite.hidden ||
        (!pathv[path_index].closed() && curve_index == 0) ||//ignore first satellites on open paths
        pathv[path_index].size() == curve_index) //ignore last satellite in open paths with fillet chamfer effect
    {
        return;
    }
    gint previous_index = curve_index - 1;
    if (curve_index == 0 && pathv[path_index].closed()) {
        previous_index = pathv[path_index].size() - 1;
    }
    if ( previous_index < 0 ) {
        return;
    }
    Geom::Curve const &curve_in = pathv[path_index][previous_index];
    double mirror_time = Geom::nearest_time(s, curve_in);
    Geom::Point mirror = curve_in.pointAt(mirror_time);
    double normal_time = Geom::nearest_time(s, pathv[path_index][curve_index]);
    Geom::Point normal = pathv[path_index][curve_index].pointAt(normal_time);
    double distance_mirror = Geom::distance(mirror,s);
    double distance_normal = Geom::distance(normal,s);
    if (Geom::are_near(s, pathv[path_index][curve_index].initialPoint(), 1.5 / _pparam->_current_zoom)) {
        satellite.amount = 0;
    } else if (distance_mirror < distance_normal) {
        double time_start = 0;
        Satellites satellites = _pparam->_last_pathvector_satellites->getSatellites();
        time_start = satellites[path_index][previous_index].time(curve_in);
        if (time_start > mirror_time) {
            mirror_time = time_start;
        }
        double size = arcLengthAt(mirror_time, curve_in);
        double amount = curve_in.length() - size;
        if (satellite.is_time) {
            amount = timeAtArcLength(amount, pathv[path_index][curve_index]);
        }
        satellite.amount = amount;
    } else {
        satellite.setPosition(s, pathv[path_index][curve_index]);
    }
    _pparam->_knot_reset_helper.clear();
    if (satellite.amount == 0) {
        char const *svgd;
        svgd = "M -5.39,8.78 -9.13,5.29 -10.38,10.28 Z M -7.22,7.07 -3.43,3.37 m -1.95,-12.16 -3.74,3.5 -1.26,-5 z "
               "m -1.83,1.71 3.78,3.7 M 5.24,8.78 8.98,5.29 10.24,10.28 Z "
               "M 7.07,7.07 3.29,3.37 M 5.24,-8.78 l 3.74,3.5 1.26,-5 z M 7.07,-7.07 3.29,-3.37";
        _pparam->_knot_reset_helper = sp_svg_read_pathv(svgd);
        _pparam->_knot_reset_helper *= Geom::Affine(_pparam->_helper_size * 0.1,0,0,_pparam->_helper_size * 0.1,0,0) * Geom::Translate(pathv[path_index][curve_index].initialPoint());
    }
    _pparam->_vector[path_index][curve_index] = satellite;
    sp_lpe_item_update_patheffect(SP_LPE_ITEM(item), false, false);
}

Geom::Point FilletChamferKnotHolderEntity::knot_get() const
{
    if (!_pparam->_last_pathvector_satellites || _pparam->_global_knot_hide) {
        return Geom::Point(Geom::infinity(), Geom::infinity());
    }
    Geom::Point tmp_point;
    size_t total_satellites = _pparam->_last_pathvector_satellites->getTotalSatellites();
    bool is_mirror = false;
    size_t index = _index;
    if (_index >= total_satellites) {
        index = _index - total_satellites;
        is_mirror = true;
    }
    std::pair<size_t, size_t> index_data = _pparam->_last_pathvector_satellites->getIndexData(index);
    size_t path_index = index_data.first;
    size_t curve_index = index_data.second;
    if (!valid_index(path_index, curve_index)) {
        return Geom::Point(Geom::infinity(), Geom::infinity());
    }
    Satellite satellite = _pparam->_vector[path_index][curve_index];
    Geom::PathVector pathv = _pparam->_last_pathvector_satellites->getPathVector();
    if (satellite.hidden ||
        (!pathv[path_index].closed() && curve_index == 0) ||//ignore first satellites on open paths
        curve_index >= pathv[path_index].size())
    {
        return Geom::Point(Geom::infinity(), Geom::infinity());
    }
    this->knot->show();
    if (is_mirror) {
        gint previous_index = curve_index - 1;
        if (curve_index == 0 && pathv[path_index].closed()) {
            previous_index = pathv[path_index].size() - 1;
        }
        if ( previous_index < 0 ) {
            return Geom::Point(Geom::infinity(), Geom::infinity());
        }
        Geom::Curve const &curve_in = pathv[path_index][previous_index];
        double s = satellite.arcDistance(pathv[path_index][curve_index]);
        double t = satellite.time(s, true, curve_in);
        if (t > 1) {
            t = 1;
        }
        if (t < 0) {
            t = 0;
        }
        double time_start = 0;
        time_start = _pparam->_last_pathvector_satellites->getSatellites()[path_index][previous_index].time(curve_in);
        if (time_start > t) {
            t = time_start;
        }
        tmp_point = (curve_in).pointAt(t);
    } else {
        tmp_point = satellite.getPosition(pathv[path_index][curve_index]);
    }
    Geom::Point const canvas_point = tmp_point;
    _pparam->updateCanvasIndicators();
    return canvas_point;
}

void FilletChamferKnotHolderEntity::knot_click(guint state)
{
    if (!_pparam->_last_pathvector_satellites) {
        return;
    }
    size_t total_satellites = _pparam->_last_pathvector_satellites->getTotalSatellites();
    bool is_mirror = false;
    size_t index = _index;
    if (_index >= total_satellites) {
        index = _index - total_satellites;
        is_mirror = true;
    }
    std::pair<size_t, size_t> index_data = _pparam->_last_pathvector_satellites->getIndexData(index);
    size_t path_index = index_data.first;
    size_t curve_index = index_data.second;
    if (!valid_index(path_index, curve_index)) {
        return;
    }
    Geom::PathVector pathv = _pparam->_last_pathvector_satellites->getPathVector();
    if ((!pathv[path_index].closed() && curve_index == 0) ||//ignore first satellites on open paths
        pathv[path_index].size() == curve_index) //ignore last satellite in open paths with fillet chamfer effect
    {
        return;
    }
    if (state & GDK_CONTROL_MASK) {
        if (state & GDK_MOD1_MASK) {
            _pparam->_vector[path_index][curve_index].amount = 0.0;
            sp_lpe_item_update_patheffect(SP_LPE_ITEM(item), false, false);
        } else {
            using namespace Geom;
            SatelliteType type = _pparam->_vector[path_index][curve_index].satellite_type;
            switch (type) {
            case FILLET:
                type = INVERSE_FILLET;
                break;
            case INVERSE_FILLET:
                type = CHAMFER;
                break;
            case CHAMFER:
                type = INVERSE_CHAMFER;
                break;
            default:
                type = FILLET;
                break;
            }
            _pparam->_vector[path_index][curve_index].satellite_type = type;
            sp_lpe_item_update_patheffect(SP_LPE_ITEM(item), false, false);
            const gchar *tip;
            if (type == CHAMFER) {
                tip = _("<b>Chamfer</b>: <b>Ctrl+Click</b> toggles type, "
                        "<b>Shift+Click</b> open dialog, "
                        "<b>Ctrl+Alt+Click</b> resets");
            } else if (type == INVERSE_CHAMFER) {
                tip = _("<b>Inverse Chamfer</b>: <b>Ctrl+Click</b> toggles type, "
                        "<b>Shift+Click</b> open dialog, "
                        "<b>Ctrl+Alt+Click</b> resets");
            } else if (type == INVERSE_FILLET) {
                tip = _("<b>Inverse Fillet</b>: <b>Ctrl+Click</b> toggles type, "
                        "<b>Shift+Click</b> open dialog, "
                        "<b>Ctrl+Alt+Click</b> resets");
            } else {
                tip = _("<b>Fillet</b>: <b>Ctrl+Click</b> toggles type, "
                        "<b>Shift+Click</b> open dialog, "
                        "<b>Ctrl+Alt+Click</b> resets");
            }
            this->knot->tip = g_strdup(tip);
            this->knot->show();
        }
    } else if (state & GDK_SHIFT_MASK) {
        double amount = _pparam->_vector[path_index][curve_index].amount;
        gint previous_index =  curve_index - 1;
        if (curve_index == 0 && pathv[path_index].closed()) {
            previous_index = pathv[path_index].size() - 1;
        }
        if ( previous_index < 0 ) {
            return;
        }
        if (!_pparam->_use_distance && !_pparam->_vector[path_index][curve_index].is_time) {
            amount = _pparam->_vector[path_index][curve_index].lenToRad(amount, pathv[path_index][previous_index], pathv[path_index][curve_index], _pparam->_vector[path_index][previous_index]);
        }
        bool aprox = false;
        Geom::D2<Geom::SBasis> d2_out = pathv[path_index][curve_index].toSBasis();
        Geom::D2<Geom::SBasis> d2_in = pathv[path_index][previous_index].toSBasis();
        aprox = ((d2_in)[0].degreesOfFreedom() != 2 ||
                 d2_out[0].degreesOfFreedom() != 2) &&
                !_pparam->_use_distance
                ? true
                : false;
        Inkscape::UI::Dialogs::FilletChamferPropertiesDialog::showDialog(
            this->desktop, amount, this, _pparam->_use_distance,
            aprox, _pparam->_vector[path_index][curve_index]);

    }
}

void FilletChamferKnotHolderEntity::knot_set_offset(Satellite satellite)
{
    if (!_pparam->_last_pathvector_satellites) {
        return;
    }
    size_t total_satellites = _pparam->_last_pathvector_satellites->getTotalSatellites();
    bool is_mirror = false;
    size_t index = _index;
    if (_index >= total_satellites) {
        index = _index - total_satellites;
        is_mirror = true;
    }
    std::pair<size_t, size_t> index_data = _pparam->_last_pathvector_satellites->getIndexData(index);
    size_t path_index = index_data.first;
    size_t curve_index = index_data.second;
    if (!valid_index(path_index, curve_index)) {
        return;
    }
    Geom::PathVector pathv = _pparam->_last_pathvector_satellites->getPathVector();
    if (satellite.hidden ||
        (!pathv[path_index].closed() && curve_index == 0) ||//ignore first satellites on open paths
        pathv[path_index].size() == curve_index) //ignore last satellite in open paths with fillet chamfer effect
    {
        return;
    }
    double amount = satellite.amount;
    double max_amount = amount;
    if (!_pparam->_use_distance && !satellite.is_time) {
        gint previous_index = curve_index - 1;
        if (curve_index == 0 && pathv[path_index].closed()) {
            previous_index = pathv[path_index].size() - 1;
        }
        if ( previous_index < 0 ) {
            return;
        }
        amount = _pparam->_vector[path_index][curve_index].radToLen(amount, pathv[path_index][previous_index], pathv[path_index][curve_index]);
        if (max_amount > 0 && amount == 0) {
            amount = _pparam->_vector[path_index][curve_index].amount;
        }
    }
    satellite.amount = amount;
    _pparam->_vector[path_index][curve_index] = satellite;
    this->parent_holder->knot_ungrabbed_handler(this->knot, 0);
    SPLPEItem *splpeitem = dynamic_cast<SPLPEItem *>(item);
    if (splpeitem) {
        sp_lpe_item_update_patheffect(splpeitem, false, false);
    }
}

} /* namespace LivePathEffect */

} /* namespace Inkscape */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
