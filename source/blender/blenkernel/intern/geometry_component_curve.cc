/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BKE_derived_curve.hh"

#include "BKE_attribute_access.hh"
#include "BKE_geometry_set.hh"

#include "attribute_access_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

CurveComponent::CurveComponent() : GeometryComponent(GEO_COMPONENT_TYPE_CURVE)
{
}

CurveComponent::~CurveComponent()
{
  this->clear();
}

GeometryComponent *CurveComponent::copy() const
{
  CurveComponent *new_component = new CurveComponent();
  if (curve_ != nullptr) {
    // new_component->curve_ = BKE_curve_copy_for_eval(curve_, false);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return new_component;
}

void CurveComponent::clear()
{
  BLI_assert(this->is_mutable());
  if (curve_ != nullptr) {
    if (ownership_ == GeometryOwnershipType::Owned) {
      delete curve_;
    }
    curve_ = nullptr;
  }
}

bool CurveComponent::has_curve() const
{
  return curve_ != nullptr;
}

/* Clear the component and replace it with the new curve. */
void CurveComponent::replace(DCurve *curve, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  curve_ = curve;
  ownership_ = ownership;
}

DCurve *CurveComponent::release()
{
  BLI_assert(this->is_mutable());
  DCurve *curve = curve_;
  curve_ = nullptr;
  return curve;
}

const DCurve *CurveComponent::get_for_read() const
{
  return curve_;
}

DCurve *CurveComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    // curve_ = BKE_curve_copy_for_eval(curve_, false);
    ownership_ = GeometryOwnershipType::Owned;
  }
  return curve_;
}

bool CurveComponent::is_empty() const
{
  return curve_ == nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attribute Access
 * \{ */

int CurveComponent::attribute_domain_size(const AttributeDomain domain) const
{
  if (curve_ == nullptr) {
    return 0;
  }
  if (domain == ATTR_DOMAIN_POINT) {
    int total = 0;
    for (const Spline *spline : curve_->splines) {
      total += spline->size();
    }
    return total;
  }
  if (domain == ATTR_DOMAIN_CURVE) {
    return curve_->splines.size();
  }
  return 0;
}

namespace blender::bke {

class BuiltinSplineAttributeProvider final : public BuiltinAttributeProvider {
  using AsReadAttribute = ReadAttributePtr (*)(const DCurve &data);
  using AsWriteAttribute = WriteAttributePtr (*)(DCurve &data);
  using UpdateOnWrite = void (*)(Spline &spline);
  const AsReadAttribute as_read_attribute_;
  const AsWriteAttribute as_write_attribute_;

 public:
  BuiltinSplineAttributeProvider(std::string attribute_name,
                                 const CustomDataType attribute_type,
                                 const CreatableEnum creatable,
                                 const WritableEnum writable,
                                 const DeletableEnum deletable,
                                 const AsReadAttribute as_read_attribute,
                                 const AsWriteAttribute as_write_attribute)
      : BuiltinAttributeProvider(std::move(attribute_name),
                                 ATTR_DOMAIN_CURVE,
                                 attribute_type,
                                 creatable,
                                 writable,
                                 deletable),
        as_read_attribute_(as_read_attribute),
        as_write_attribute_(as_write_attribute)
  {
  }

  ReadAttributePtr try_get_for_read(const GeometryComponent &component) const final
  {
    const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
    const DCurve *curve = curve_component.get_for_read();
    if (curve == nullptr) {
      return {};
    }

    return as_read_attribute_(*curve);
  }

  WriteAttributePtr try_get_for_write(GeometryComponent &component) const final
  {
    if (writable_ != Writable) {
      return {};
    }
    CurveComponent &curve_component = static_cast<CurveComponent &>(component);
    DCurve *curve = curve_component.get_for_write();
    if (curve == nullptr) {
      return {};
    }

    return as_write_attribute_(*curve);
  }

  bool try_delete(GeometryComponent &UNUSED(component)) const final
  {
    return false;
  }

  bool try_create(GeometryComponent &UNUSED(component)) const final
  {
    return false;
  }

  bool exists(const GeometryComponent &component) const final
  {
    return component.attribute_domain_size(ATTR_DOMAIN_CURVE) != 0;
  }
};

static int get_spline_resolution(Spline *const &spline)
{
  return spline->resolution();
}

static void set_spline_resolution(Spline *&spline, const int &resolution)
{
  spline->set_resolution(std::max(resolution, 1));
  spline->mark_cache_invalid();
}

static ReadAttributePtr make_resolution_read_attribute(const DCurve &curve)
{
  return std::make_unique<DerivedArrayReadAttribute<Spline *, int, get_spline_resolution>>(
      ATTR_DOMAIN_CURVE, curve.splines.as_span());
}

static WriteAttributePtr make_resolution_write_attribute(DCurve &curve)
{
  return std::make_unique<
      DerivedArrayWriteAttribute<Spline *, int, get_spline_resolution, set_spline_resolution>>(
      ATTR_DOMAIN_CURVE, curve.splines.as_mutable_span());
}

static float get_spline_length(Spline *const &spline)
{
  Span<float3> positions = spline->evaluated_positions();
  if (positions.is_empty()) {
    return 0.0f;
  }

  float length = 0.0f;
  for (const int i : IndexRange(1, positions.size() - 1)) {
    length += float3::distance(positions[i], positions[i - 1]);
  }

  return length;
}

static ReadAttributePtr make_length_attribute(const DCurve &curve)
{
  return std::make_unique<DerivedArrayReadAttribute<Spline *, float, get_spline_length>>(
      ATTR_DOMAIN_CURVE, curve.splines.as_span());
}

static bool get_cyclic_value(Spline *const &spline)
{
  return spline->is_cyclic;
}

static void set_cyclic_value(Spline *&spline, const bool &value)
{
  spline->is_cyclic = value;
  spline->mark_cache_invalid();
}

static ReadAttributePtr make_cyclic_read_attribute(const DCurve &curve)
{
  return std::make_unique<DerivedArrayReadAttribute<Spline *, bool, get_cyclic_value>>(
      ATTR_DOMAIN_CURVE, curve.splines.as_span());
}

static WriteAttributePtr make_cyclic_write_attribute(DCurve &curve)
{
  return std::make_unique<
      DerivedArrayWriteAttribute<Spline *, bool, get_cyclic_value, set_cyclic_value>>(
      ATTR_DOMAIN_CURVE, curve.splines.as_mutable_span());
}

/**
 * In this function all the attribute providers for a curve component are created. Most data
 * in this function is statically allocated, because it does not change over time.
 */
static ComponentAttributeProviders create_attribute_providers_for_curve()
{
  static BuiltinSplineAttributeProvider resolution("resolution",
                                                   CD_PROP_INT32,
                                                   BuiltinAttributeProvider::NonCreatable,
                                                   BuiltinAttributeProvider::Writable,
                                                   BuiltinAttributeProvider::NonDeletable,
                                                   make_resolution_read_attribute,
                                                   make_resolution_write_attribute);

  static BuiltinSplineAttributeProvider length("length",
                                               CD_PROP_FLOAT,
                                               BuiltinAttributeProvider::NonCreatable,
                                               BuiltinAttributeProvider::Readonly,
                                               BuiltinAttributeProvider::NonDeletable,
                                               make_length_attribute,
                                               nullptr);

  static BuiltinSplineAttributeProvider cyclic("cyclic",
                                               CD_PROP_BOOL,
                                               BuiltinAttributeProvider::NonCreatable,
                                               BuiltinAttributeProvider::Writable,
                                               BuiltinAttributeProvider::NonDeletable,
                                               make_cyclic_read_attribute,
                                               make_cyclic_write_attribute);

  return ComponentAttributeProviders({&resolution, &length, &cyclic}, {});
}

}  // namespace blender::bke

const blender::bke::ComponentAttributeProviders *CurveComponent::get_attribute_providers() const
{
  static blender::bke::ComponentAttributeProviders providers =
      blender::bke::create_attribute_providers_for_curve();
  return &providers;
}

/** \} */
