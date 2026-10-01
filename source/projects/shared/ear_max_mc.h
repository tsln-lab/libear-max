/// @file
/// @brief   Helpers shared by the multichannel (mc.*) externals: per-input
///          addressing of parameters and the Max C API methods for
///          multichannel outlets.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "c74_min.h"

#include <string>

namespace earmax {

using namespace c74::min;

/// Parse the first atom of a `setvalue` message as an input index: 1-based,
/// 0 meaning all inputs (as in Max's mc objects). Returns false on error.
template <class error_fn>
bool parse_mc_index(const atoms& args, size_t count, long& index, error_fn&& error)
{
    if (args.empty() || !atom_is_numeric(args[0])) {
        error("setvalue needs an input number (1-based, 0 for all) followed by a parameter");
        return false;
    }
    index = static_cast<long>(static_cast<double>(args[0]));
    if (index < 0 || static_cast<size_t>(index) > count) {
        error("setvalue: input number out of range 0.." + std::to_string(count));
        return false;
    }
    return true;
}

/// Max calls this to learn the channel count of a multichannel outlet; the
/// Min object must provide `long mc_output_channels(long outlet)`.
template <class min_class_type>
long mc_multichanneloutputs(c74::max::t_object* x, long outlet_index)
{
    auto* self = reinterpret_cast<minwrap<min_class_type>*>(x);
    return self->m_min_object.mc_output_channels(outlet_index);
}

/// Max calls this when the channel count of a multichannel inlet changes;
/// the Min object must provide `long mc_input_changed(long inlet, long channels)`
/// and return non-zero if its output channel count changed as a result.
template <class min_class_type>
long mc_inputchanged(c74::max::t_object* x, long inlet_index, long channels)
{
    auto* self = reinterpret_cast<minwrap<min_class_type>*>(x);
    return self->m_min_object.mc_input_changed(inlet_index, channels);
}

/// Register the multichannel methods on the Max class (call from maxclass_setup).
template <class min_class_type>
void mc_register_methods(c74::max::t_class* c)
{
    c74::max::class_addmethod(c, reinterpret_cast<c74::max::method>(mc_multichanneloutputs<min_class_type>),
                              "multichanneloutputs", c74::max::A_CANT, 0);
    c74::max::class_addmethod(c, reinterpret_cast<c74::max::method>(mc_inputchanged<min_class_type>),
                              "inputchanged", c74::max::A_CANT, 0);
}

} // namespace earmax
