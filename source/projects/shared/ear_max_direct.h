/// @file
/// @brief   DirectSpeakers metadata with name-based parameters, shared by the
///          DirectSpeakers externals.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "ear_max.h"

#include <cmath>

namespace earmax {

/// ADM DirectSpeakers metadata for one channel, with parameters applied by name.
struct direct_metadata {
    ear::DirectSpeakersTypeMetadata dstm;
    ear::PolarSpeakerPosition position;
    /// the position is Cartesian (X Y Z in the ADM's cube, as Dolby Atmos
    /// masters give their beds) instead of polar
    bool cartesian{ false };
    ear::CartesianSpeakerPosition cartesian_position;
    /// polar: azimuthMin azimuthMax elevationMin elevationMax [distanceMin distanceMax];
    /// Cartesian: XMin XMax YMin YMax ZMin ZMax; or empty
    std::vector<double> bounds;

    direct_metadata()
    {
        rebuild_position();
    }

    void rebuild_position()
    {
        if (cartesian) {
            ear::CartesianSpeakerPosition p(cartesian_position.X, cartesian_position.Y, cartesian_position.Z);
            if (bounds.size() >= 6) {
                p.XMin = bounds[0];
                p.XMax = bounds[1];
                p.YMin = bounds[2];
                p.YMax = bounds[3];
                p.ZMin = bounds[4];
                p.ZMax = bounds[5];
            }
            dstm.position = p;
            return;
        }
        ear::PolarSpeakerPosition p(position.azimuth, position.elevation, position.distance);
        if (bounds.size() >= 4) {
            p.azimuthMin = bounds[0];
            p.azimuthMax = bounds[1];
            p.elevationMin = bounds[2];
            p.elevationMax = bounds[3];
        }
        if (bounds.size() >= 6) {
            p.distanceMin = bounds[4];
            p.distanceMax = bounds[5];
        }
        dstm.position = p;
    }

    static const char* parameter_names()
    {
        return "speakerlabel cartesian position azimuth elevation distance x y z bounds lfe packformat";
    }

    /// Apply a parameter by name; see object_metadata::apply.
    template <class error_fn>
    bool apply(const std::string& name, const atoms& args, error_fn&& error)
    {
        auto need_number = [&](double& out) {
            if (args.empty() || !atom_is_numeric(args[0])) {
                error(name + " needs a number");
                return false;
            }
            const double v = static_cast<double>(args[0]);
            if (!std::isfinite(v)) {
                error(name + " needs a finite number");
                return false;
            }
            out = v;
            return true;
        };

        if (name == "speakerlabel") {
            dstm.speakerLabels.clear();
            for (const auto& a : args) {
                dstm.speakerLabels.push_back(std::string(a));
            }
        }
        else if (name == "azimuth") {
            if (!need_number(position.azimuth)) return false;
            rebuild_position();
        }
        else if (name == "elevation") {
            if (!need_number(position.elevation)) return false;
            rebuild_position();
        }
        else if (name == "distance") {
            if (!need_number(position.distance)) return false;
            rebuild_position();
        }
        else if (name == "cartesian") {
            double v = 0.0;
            if (!need_number(v)) return false;
            const bool flag = v != 0.0;
            if (flag != cartesian) {
                bounds.clear();    // the bounds belong to the other coordinate system
            }
            cartesian = flag;
            rebuild_position();
        }
        else if (name == "x") {
            if (!need_number(cartesian_position.X)) return false;
            rebuild_position();
        }
        else if (name == "y") {
            if (!need_number(cartesian_position.Y)) return false;
            rebuild_position();
        }
        else if (name == "z") {
            if (!need_number(cartesian_position.Z)) return false;
            rebuild_position();
        }
        else if (name == "position" || name == "list") {
            if (args.size() < (cartesian ? 3u : 2u)) {
                error(cartesian ? "position needs 3 numbers: x y z (cartesian is on)"
                                : "position needs at least 2 numbers: azimuth elevation [distance]");
                return false;
            }
            for (const auto& a : args) {
                if (!atom_is_numeric(a) || !std::isfinite(static_cast<double>(a))) {
                    error("position values must be finite numbers");
                    return false;
                }
            }
            if (cartesian) {
                cartesian_position.X = static_cast<double>(args[0]);
                cartesian_position.Y = static_cast<double>(args[1]);
                cartesian_position.Z = static_cast<double>(args[2]);
            }
            else {
                position.azimuth = static_cast<double>(args[0]);
                position.elevation = static_cast<double>(args[1]);
                if (args.size() > 2) position.distance = static_cast<double>(args[2]);
            }
            rebuild_position();
        }
        else if (name == "bounds") {
            if (cartesian && !args.empty() && args.size() != 6) {
                error("bounds needs 6 numbers: XMin XMax YMin YMax ZMin ZMax (cartesian is on), or none to clear");
                return false;
            }
            if (!cartesian && !args.empty() && args.size() != 4 && args.size() != 6) {
                error("bounds needs 4 or 6 numbers: azimuthMin azimuthMax elevationMin elevationMax [distanceMin distanceMax], or none to clear");
                return false;
            }
            std::vector<double> values;
            for (const auto& a : args) {
                if (!atom_is_numeric(a) || !std::isfinite(static_cast<double>(a))) {
                    error("bounds values must be finite numbers");
                    return false;
                }
                values.push_back(static_cast<double>(a));
            }
            bounds = values;
            rebuild_position();
        }
        else if (name == "lfe") {
            if (args.empty() || !atom_is_numeric(args[0])) {
                error("lfe needs 0 or 1");
                return false;
            }
            if (static_cast<double>(args[0]) != 0.0) {
                dstm.channelFrequency.lowPass = 120.0;
            }
            else {
                dstm.channelFrequency.lowPass = boost::none;
            }
        }
        else if (name == "packformat") {
            const std::string id = args.empty() ? std::string() : std::string(args[0]);
            if (id.empty() || id == "none") {
                dstm.audioPackFormatID = boost::none;
            }
            else {
                dstm.audioPackFormatID = id;
            }
        }
        else {
            error("unknown channel parameter '" + name + "'; parameters are: " + parameter_names());
            return false;
        }
        return true;
    }
};

/// Calculate gains for one DirectSpeakers channel with a shared calculator.
template <class report_fn>
bool compute_direct_gains(ear::GainCalculatorDirectSpeakers& calc, const ear::DirectSpeakersTypeMetadata& dstm,
                          std::vector<float>& gains, report_fn&& report)
{
    try {
        calc.calculate(dstm, gains, [&](const ear::Warning& warning) { report("warning: " + warning.message); });
        return true;
    }
    catch (const std::exception& e) {
        report(e.what());
        return false;
    }
}

} // namespace earmax
