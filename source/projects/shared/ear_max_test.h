/// @file
/// @brief   Test helpers shared by the libear-max unit tests.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "c74_min_unittest.h"
#include "ear_max_doc.h"

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace earmax_test {

using namespace c74::min;

/// Index of the first `@attribute` argument (like Max's attr_args_offset(),
/// which the mock kernel stubs out to always return 0).
inline long attr_args_offset(const long ac, const c74::max::t_atom* av)
{
    for (long i = 0; i < ac; ++i) {
        if (av[i].a_type == c74::max::A_SYM && av[i].a_w.w_sym && av[i].a_w.w_sym->s_name[0] == '@') {
            return i;
        }
    }
    return ac;
}

/// The values of the most recent list sent from an outlet (selector symbols are skipped).
inline std::vector<double> last_list(void* obj, int outlet_index)
{
    auto& output = *c74::max::object_getoutput(obj, outlet_index);
    REQUIRE(!output.empty());
    const auto& msg = output.back();
    std::vector<double> values;
    for (const auto& a : msg) {
        if (a.a_type == c74::max::A_SYM) {
            continue;
        }
        values.push_back(static_cast<double>(a));
    }
    return values;
}

/// RAII instance of a Min object created with box arguments.
/// Requires EARMAX_TEST_FORWARD_ARGUMENTS(cls) in the test translation unit.
template <class min_class_type>
class test_wrapper_args {
public:
    explicit test_wrapper_args(const atoms& args)
    {
        m_wrapped = wrapper_new<min_class_type>(symbol("test"), static_cast<long>(args.size()), args.data());
    }

    ~test_wrapper_args()
    {
        c74::max::object_free(m_wrapped);
    }

    operator min_class_type&()
    {
        return m_wrapped->m_min_object;
    }

private:
    minwrap<min_class_type>* m_wrapped{ nullptr };
};

} // namespace earmax_test

/// Specialise Min's wrapper_new() for one class so that box arguments reach
/// the constructor under the mock kernel. wrapper_new() is a friend of
/// object_base, which is what lets this touch the private initialisation
/// steps. Must appear after the class definition and before MIN_EXTERNAL().
#define EARMAX_TEST_FORWARD_ARGUMENTS(cls)                                                                            \
    namespace c74::min {                                                                                              \
    template <>                                                                                                       \
    minwrap<cls>* wrapper_new<cls>(const max::t_symbol* name, const long ac, const max::t_atom* av)                   \
    {                                                                                                                 \
        const atom_reference args(ac, av);                                                                            \
        const long attrstart = earmax_test::attr_args_offset(ac, av);                                                 \
        auto self = static_cast<minwrap<cls>*>(max::object_alloc(this_class));                                        \
        auto self_ob = reinterpret_cast<max::t_object*>(self);                                                        \
        self->m_min_object.assign_instance(self_ob);                                                                  \
        min_ctor<cls>(self, atoms(args.begin(), args.begin() + attrstart));                                           \
        self->m_min_object.postinitialize();                                                                          \
        self->m_min_object.set_classname(name);                                                                       \
        self->setup();                                                                                                \
        self->m_min_object.try_call("setup");                                                                         \
        max::object_attach_byptr_register(self, self, k_sym_box);                                                     \
        max::attr_args_process(self, static_cast<short>(args.size()), const_cast<max::t_atom*>(args.begin()));        \
        return self;                                                                                                  \
    }                                                                                                                 \
    }

/// Generate the object's reference page, docs/<name>.maxref.xml in the
/// package (EARMAX_DOCS_DIR, set by CMake), with the generator in
/// ear_max_doc.h. The pages are committed and shipped with the package, so
/// after changing a description run the tests and commit the regenerated
/// page; CI checks that the committed pages are current and well-formed
/// (tools/check_maxref.py). The docs folder is part of the repository (no
/// std::filesystem here: it needs macOS 10.15, the externals target 10.11).
#define EARMAX_TEST_GENERATE_MAXREF(cls, max_name)                                                                     \
    TEST_CASE("reference page for " max_name " is generated")                                                          \
    {                                                                                                                  \
        ext_main(nullptr);                                                                                             \
        test_wrapper<cls> an_instance;                                                                                 \
        cls& obj = an_instance;                                                                                        \
        const std::string file = std::string(EARMAX_DOCS_DIR) + "/" + max_name + ".maxref.xml";                        \
        earmax::doc::generate<cls>(obj, file, max_name);                                                               \
        std::ifstream check(file);                                                                                     \
        REQUIRE(check.good());                                                                                         \
    }
