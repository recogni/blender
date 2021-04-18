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

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_curve_trim_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_FLOAT, N_("Start"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
    {SOCK_FLOAT, N_("Start"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_DISTANCE},
    {SOCK_FLOAT, N_("End"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
    {SOCK_FLOAT, N_("End"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_DISTANCE},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_curve_trim_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_curve_trim_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void geo_node_curve_trim_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveTrim *data = (NodeGeometryCurveTrim *)MEM_callocN(sizeof(NodeGeometryCurveTrim),
                                                                     __func__);

  data->mode = GEO_NODE_CURVE_TRIM_FACTOR;
  node->storage = data;
}

static void geo_node_curve_trim_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryCurveTrim &node_storage = *(NodeGeometryCurveTrim *)node->storage;
  const GeometryNodeCurveTrimMode mode = (GeometryNodeCurveTrimMode)node_storage.mode;

  bNodeSocket *factor_start_socket = ((bNodeSocket *)node->inputs.first)->next;
  bNodeSocket *length_start_socket = factor_start_socket->next;
  bNodeSocket *factor_end_socket = length_start_socket->next;
  bNodeSocket *length_end_socket = factor_end_socket->next;

  nodeSetSocketAvailability(factor_start_socket, mode == GEO_NODE_CURVE_TRIM_FACTOR);
  nodeSetSocketAvailability(length_start_socket, mode == GEO_NODE_CURVE_TRIM_LENGTH);
  nodeSetSocketAvailability(factor_end_socket, mode == GEO_NODE_CURVE_TRIM_FACTOR);
  nodeSetSocketAvailability(length_end_socket, mode == GEO_NODE_CURVE_TRIM_LENGTH);
}

namespace blender::nodes {

static void interpolate_control_point(Spline &spline, const Spline::LookupResult lookup)
{
  const int evaluated_index = lookup.evaluated_index;
  Span<PointMapping> mappings = spline.evaluated_mappings();
  const PointMapping &mapping = mappings[evaluated_index];
  const int index = mapping.control_point_index;

  Span<float3> evaluated_positions = spline.evaluated_positions();

  const float3 new_position = float3::interpolate(evaluated_positions[evaluated_index],
                                                  evaluated_positions[evaluated_index + 1],
                                                  lookup.factor);
  if (BezierSpline *bezier_spline = dynamic_cast<BezierSpline *>(&spline)) {
    /* TODO: This could be converted to a virtual function in the Spline class. */
    bezier_spline->move_control_point(index, new_position);
  }
  else {
    spline.positions()[index] = new_position;
  }

  /* TODO: Do this interpolation with attributes instead. */

  MutableSpan<float> radii = spline.radii();
  Array<float, 2> neighboring_radii(2);
  spline.interpolate_data_to_evaluated_points(
      radii.as_span(), neighboring_radii.as_mutable_span(), evaluated_index);
  radii[index] = interpf(neighboring_radii[1], neighboring_radii[0], lookup.factor);

  MutableSpan<float> tilts = spline.tilts();
  Array<float, 2> neighboring_tilts(2);
  spline.interpolate_data_to_evaluated_points(
      tilts.as_span(), neighboring_tilts.as_mutable_span(), evaluated_index);
  tilts[index] = interpf(neighboring_tilts[1], neighboring_tilts[0], lookup.factor);

  /* Interpolate information specific to different spline types. */
  if (NURBSpline *poly_spline = dynamic_cast<NURBSpline *>(&spline)) {
    MutableSpan<float> weights = poly_spline->weights();
    Array<float, 2> neighboring_weights(2);
    spline.interpolate_data_to_evaluated_points(
        weights.as_span(), neighboring_weights.as_mutable_span(), evaluated_index);
    weights[index] = interpf(neighboring_weights[1], neighboring_weights[0], lookup.factor);
  }
}

static void trim_spline(Spline &spline,
                        const Spline::LookupResult start,
                        const Spline::LookupResult end)
{
  BLI_assert(!spline.is_cyclic);
  BLI_assert(start.evaluated_index <= end.evaluated_index);

  Span<PointMapping> mappings = spline.evaluated_mappings();

  const int points_len = spline.size();
  const int start_index = mappings[start.evaluated_index].control_point_index;
  const int end_index = std::min(mappings[end.evaluated_index].control_point_index + 1,
                                 points_len - 1);

  if (!(start.evaluated_index == 0 && start.factor == 0.0f)) {
    interpolate_control_point(spline, start);
  }
  if (end.evaluated_index != spline.evaluated_points_size() - 1) {
    interpolate_control_point(spline, end);
  }

  spline.drop_back(std::min(points_len - end_index, points_len));
  spline.drop_front(std::max(start_index - 1, 0));
}

static void geo_node_curve_trim_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  const bNode &node = params.node();
  const NodeGeometryCurveTrim &node_storage = *(const NodeGeometryCurveTrim *)node.storage;
  const GeometryNodeCurveTrimMode mode = (GeometryNodeCurveTrimMode)node_storage.mode;

  if (!geometry_set.has_curve()) {
    params.set_output("Geometry", geometry_set);
    return;
  }

  DCurve &curve = *geometry_set.get_curve_for_write();

  switch (mode) {
    case GEO_NODE_CURVE_TRIM_FACTOR: {
      const float factor_start = params.extract_input<float>("Start");
      const float factor_end = params.extract_input<float>("End");
      for (SplinePtr &spline : curve.splines) {
        if (spline->is_cyclic) {
          continue;
        }
        trim_spline(*spline,
                    spline->lookup_evaluated_factor(std::min(factor_start, factor_end)),
                    spline->lookup_evaluated_factor(std::max(factor_start, factor_end)));
      }
      break;
    }
    case GEO_NODE_CURVE_TRIM_LENGTH: {
      const float length_start = params.extract_input<float>("Start_001");
      const float length_from_end = params.extract_input<float>("End_001");
      for (SplinePtr &spline : curve.splines) {
        if (spline->is_cyclic) {
          continue;
        }
        const float length_end = spline->length() - length_from_end;
        trim_spline(*spline,
                    spline->lookup_evaluated_length(std::min(length_start, length_end)),
                    spline->lookup_evaluated_length(std::max(length_start, length_end)));
      }
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_curve_trim()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CURVE_TRIM, "Curve Trim", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_curve_trim_in, geo_node_curve_trim_out);
  node_type_init(&ntype, geo_node_curve_trim_init);
  node_type_update(&ntype, geo_node_curve_trim_update);
  node_type_storage(
      &ntype, "NodeGeometryCurveTrim", node_free_standard_storage, node_copy_standard_storage);

  ntype.geometry_node_execute = blender::nodes::geo_node_curve_trim_exec;
  ntype.draw_buttons = geo_node_curve_trim_layout;
  nodeRegisterType(&ntype);
}
