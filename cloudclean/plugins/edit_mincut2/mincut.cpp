#include "mincut.h"

#include <pcl/segmentation/boost.h>
#include <pcl/search/search.h>
#include <pcl/search/kdtree.h>
#include <stdlib.h>
#include <cmath>
#include <QDebug>
#include <string>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MinCut::MinCut () :
  inverse_sigma_ (16.0),
  binary_potentials_are_valid_ (false),
  epsilon_ (0.0001),
  radius_ (16.0),
  unary_potentials_are_valid_ (false),
  source_weight_ (0.8),
  search_ (),
  number_of_neighbours_ (14),
  graph_is_valid_ (false),
  foreground_points_ (0),
  background_points_ (0),
  clusters_ (0),
  graph_ (),
  capacity_ (),
  reverse_edges_ (),
  vertices_ (0),
  edge_marker_ (0),
  source_ (),/////////////////////////////////
  sink_ (),///////////////////////////////////
  max_flow_ (0.0)
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MinCut::~MinCut ()
{
  if (search_ != 0)
    search_.reset ();
  if (graph_ != 0)
    graph_.reset ();
  if (capacity_ != 0)
    capacity_.reset ();
  if (reverse_edges_ != 0)
    reverse_edges_.reset ();

  foreground_points_.clear ();
  background_points_.clear ();
  clusters_.clear ();
  vertices_.clear ();
  edge_marker_.clear ();
  input_.reset ();
  indices_.reset ();
}

boost::shared_ptr<MinCut::gData> MinCut::getGraphData(){
    qDebug("get graph data called!");
    boost::shared_ptr<MinCut::gData> data = boost::shared_ptr<MinCut::gData>(new MinCut::gData());

    // Finds all points that are indexed
    std::vector<int> labels;
    labels.resize (input_->points.size (), 0);
    int number_of_indices = static_cast<int> (indices_->size ());
    for (int i_point = 0; i_point < number_of_indices; i_point++)
        labels[(*indices_)[i_point]] = 1;

    // Note: Only valid after extract was run
    ResidualCapacityMap residual_capacity = boost::get (boost::edge_residual_capacity, *graph_);

    // temporary vertices
    std::vector<int> tmp_vertices;
    std::map<int, int> tmp_labels;

    // Set vertices
    OutEdgeIterator edge_iter, edge_end;
    // for every edge connected to the source vertex (all of them)
    for ( boost::tie (edge_iter, edge_end) = boost::out_edges (source_, *graph_); edge_iter != edge_end; edge_iter++ )
    {
      if (labels[edge_iter->m_target] == 1){
        if (residual_capacity[*edge_iter] > epsilon_){
            data->source_vertices.push_back(edge_iter->m_target);
            tmp_labels.insert(std::make_pair(edge_iter->m_target, 0));
        }
        else{
            data->sink_vertices.push_back(edge_iter->m_target);
            tmp_labels.insert(std::make_pair(edge_iter->m_target, 1));
        }
        tmp_vertices.push_back(edge_iter->m_target);
      }
    }


    // Set up edges

    // checks for duplicates
    std::set<std::pair<int, int> > edge_marker;

    // Find maximum edge weight

    float max_weight = 0;

    //qDebug("Viz!!");

    // for all the vertices
    for(int i = 0; i < tmp_vertices.size(); i++){
        int idx = tmp_vertices[i];
        // For every neighbour edge
        for ( boost::tie (edge_iter, edge_end) = boost::out_edges (idx, *graph_); edge_iter != edge_end; edge_iter++ ) {
            VertexDescriptor target = edge_iter->m_target;
            if(target == source_ || target == sink_)
                continue;

            // Set up edge source and est
            int a = idx, b = static_cast<int>(target);
            assert(a !=b);
            // swap if a is bigger
            if(a > b){ int tmp = a; a = b; b = tmp;}
            std::pair<int, int> edge = std::make_pair(a, b);
            auto retpair = edge_marker.insert(edge);

            // Prevent duplicates, continue the edge was previously inserted
            if(!retpair.second)
                continue;

            //float weight = residual_capacity[*edge_iter];

            float weight_forward = (*capacity_)[*edge_iter];
            float weight_reverse = (*capacity_)[(*reverse_edges_)[*edge_iter]];
            float weight = weight_forward + weight_reverse;

            if(weight > max_weight)
                max_weight = weight;

            //std::string label = "None";

            // i think here i'm passing the absolute index and not the index buffer index
            // Determine label
            if(tmp_labels[a] == tmp_labels[b]){
                if(tmp_labels[idx] == 0){
                    data->source_edges.push_back(edge);
                    data->source_edge_weights.push_back(weight);
                    //label = "source";
                }
                else{
                    data->sink_edges.push_back(edge);
                    data->sink_edge_weights.push_back(weight);
                    //label = "sink";
                }
            }
            else{
                data->bridge_edges.push_back(edge);
                data->bridge_edge_weights.push_back(weight);
                //label = "bridge";
            }

            //qDebug("Edge from %d to %d, weight = %f, label = %s", a, b, weight, label.c_str());

            // what about the flow?
        }
    }

    // normalise weights
    for(int i = 0; i < data->source_edge_weights.size(); i++)
        data->source_edge_weights[i] /= max_weight;
    for(int i = 0; i < data->sink_edge_weights.size(); i++)
        data->sink_edge_weights[i] /= max_weight;
    for(int i = 0; i < data->bridge_edge_weights.size(); i++)
        data->bridge_edge_weights[i] /= max_weight;



    return data;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 void
MinCut::setInputCloud (PointCloud::Ptr &cloud)
{
  input_ = cloud;
  graph_is_valid_ = false;
  unary_potentials_are_valid_ = false;
  binary_potentials_are_valid_ = false;
}


void MinCut::setBoundingPolygon(std::vector<Eigen::Vector3f> &polygon, Eigen::Vector3f &center){
    polygon_ = polygon;
    subcloud_center_ = center;
}

void MinCut::setCameraOrigin(Eigen::Vector3f origin){
    cam_origin_ = origin;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 double
MinCut::getSigma () const
{
  return (pow (1.0 / inverse_sigma_, 0.5));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 void
MinCut::setSigma (double sigma)
{
  if (sigma > epsilon_)
  {
    inverse_sigma_ = 1.0 / (sigma * sigma);
    binary_potentials_are_valid_ = false;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 double
MinCut::getRadius () const
{
  return (pow (radius_, 0.5));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 void
MinCut::setRadius (double radius)
{
  if (radius > epsilon_)
  {
    radius_ = radius * radius;
    unary_potentials_are_valid_ = false;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 double
MinCut::getSourceWeight () const
{
  return (source_weight_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 void
MinCut::setSourceWeight (double weight)
{
  if (weight > epsilon_)
  {
    source_weight_ = weight;
    unary_potentials_are_valid_ = false;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 typename MinCut::KdTreePtr
MinCut::getSearchMethod () const
{
  return (search_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 void
MinCut::setSearchMethod (const KdTreePtr& tree)
{
  if (search_ != 0)
    search_.reset ();

  search_ = tree;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 unsigned int
MinCut::getNumberOfNeighbours () const
{
  return (number_of_neighbours_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 void
MinCut::setNumberOfNeighbours (unsigned int neighbour_number)
{
  if (number_of_neighbours_ != neighbour_number && neighbour_number != 0)
  {
    number_of_neighbours_ = neighbour_number;
    graph_is_valid_ = false;
    unary_potentials_are_valid_ = false;
    binary_potentials_are_valid_ = false;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 std::vector<pcl::PointXYZI, Eigen::aligned_allocator<pcl::PointXYZI> >
MinCut::getForegroundPoints () const
{
  return (foreground_points_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 void
MinCut::setForegroundPoints (typename pcl::PointCloud<pcl::PointXYZI>::Ptr foreground_points)
{
  foreground_points_.clear ();
  foreground_points_.reserve (foreground_points->points.size ());
  for (size_t i_point = 0; i_point < foreground_points->points.size (); i_point++)
    foreground_points_.push_back (foreground_points->points[i_point]);

  unary_potentials_are_valid_ = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 void
MinCut::extract (std::vector <pcl::PointIndices>& clusters)
{
  clusters.clear ();

  bool segmentation_is_possible = initCompute ();
  if ( !segmentation_is_possible )
  {
    deinitCompute ();
    return;
  }

  // copy was here

  clusters_.clear ();
  bool success = true;

  if ( !graph_is_valid_ )
  {
    success = buildGraph ();
    if (success == false)
    {
      deinitCompute ();
      return;
    }
    graph_is_valid_ = true;
    unary_potentials_are_valid_ = true;
    binary_potentials_are_valid_ = true;
  }

  if ( !unary_potentials_are_valid_ )
  {
    success = recalculateUnaryPotentials ();
    if (success == false)
    {
      deinitCompute ();
      return;
    }
    unary_potentials_are_valid_ = true;
  }

  if ( !binary_potentials_are_valid_ )
  {
    success = recalculateBinaryPotentials ();
    if (success == false)
    {
      deinitCompute ();
      return;
    }
    binary_potentials_are_valid_ = true;
  }

  // All checks are done here

  //IndexMap index_map = boost::get (boost::vertex_index, *graph_);
  ResidualCapacityMap residual_capacity = boost::get (boost::edge_residual_capacity, *graph_);

  max_flow_ = boost::boykov_kolmogorov_max_flow (*graph_, source_, sink_);

  assembleLabels (residual_capacity);

  // Copy current clusters to input ref?? WHY?
  if ( graph_is_valid_ && unary_potentials_are_valid_ && binary_potentials_are_valid_ )
  {
    clusters.reserve (clusters_.size ());
    std::copy (clusters_.begin (), clusters_.end (), std::back_inserter (clusters));
    deinitCompute ();
    return;
  }

  deinitCompute ();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 double
MinCut::getMaxFlow () const
{
  return (max_flow_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 typename boost::shared_ptr<typename MinCut::mGraph>
MinCut::getGraph () const
{
  return (graph_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 bool
MinCut::buildGraph ()
{
  int number_of_points = static_cast<int> (input_->points.size ());
  int number_of_indices = static_cast<int> (indices_->size ());

  if (input_->points.size () == 0 || number_of_points == 0 /*|| foreground_points_.empty () == true*/ )
    return (false);

  if (search_ == 0)
    search_ = boost::shared_ptr<pcl::search::Search<pcl::PointXYZI> > (new pcl::search::KdTree<pcl::PointXYZI>);

  graph_.reset ();
  graph_ = boost::shared_ptr< mGraph > (new mGraph ());

  // Checks done, empty graph

  capacity_.reset ();
  capacity_ = boost::shared_ptr<CapacityMap> (new CapacityMap ());
  *capacity_ = boost::get (boost::edge_capacity, *graph_);

  // Capacity map configured

  reverse_edges_.reset ();
  reverse_edges_ = boost::shared_ptr<ReverseEdgeMap> (new ReverseEdgeMap ());
  *reverse_edges_ = boost::get (boost::edge_reverse, *graph_);

  // Reverse edge map configured

  VertexDescriptor vertex_descriptor(0);
  vertices_.clear ();
  vertices_.resize (number_of_points + 2, vertex_descriptor);

  // Added 2 new vertices with 0 descriptors

  std::set<int> out_edges_marker;
  edge_marker_.clear ();
  edge_marker_.resize (number_of_points + 2, out_edges_marker);

  // Added two new out edge markers? avoid duplicate adds

  // Add vertices who have default indices (I assume)
  for (int i_point = 0; i_point < number_of_points + 2; i_point++)
    vertices_[i_point] = boost::add_vertex (*graph_);

  // Last two indices are the source and sink
  source_ = vertices_[number_of_points];
  sink_ = vertices_[number_of_points + 1];

  // Link up every point to the source and sink, with weights
  for (int i_point = 0; i_point < number_of_indices; i_point++)
  {
    int point_index = (*indices_)[i_point];
    double source_weight = 0.0; // Foreground penalty
    double sink_weight = 0.0; // Background penalty
    calculateUnaryPotential (point_index, source_weight, sink_weight);
    addEdge (static_cast<int> (source_), point_index, source_weight); // Connect to source
    addEdge (point_index, static_cast<int> (sink_), sink_weight); // Connect to sink
  }

  // Set the neighbours for every point
  // These are binary edges with weights
  std::vector<int> neighbours;
  std::vector<float> distances;
  search_->setInputCloud (input_, indices_);
  for (int i_point = 0; i_point < number_of_indices; i_point++)
  {
    int point_index = (*indices_)[i_point];
    search_->nearestKSearch (i_point, number_of_neighbours_, neighbours, distances);
    //qDebug("Neighbour count: %d", neighbours.size());
    for (size_t i_nghbr = 1; i_nghbr < neighbours.size (); i_nghbr++) // WHY skip the first neighbour? Exclude itself?
    {
      double weight = calculateBinaryPotential (point_index, neighbours[i_nghbr]);
      addEdge (point_index, neighbours[i_nghbr], weight);
      addEdge (neighbours[i_nghbr], point_index, weight);

      int from = vertices_[neighbours[i_nghbr]];
      int to = vertices_[point_index];
      if(from > to){int tmp = from; from = to; to = tmp;}
      //qDebug("Edge from %d to %d, weight = %f", from, to, weight);

    }
    neighbours.clear ();
    distances.clear ();
  }

  return (true);
}

inline Eigen::Vector3f pointOnPlane(Eigen::Vector4f & plane){
    Eigen::Vector3f point_on_plane(0,0,0);

    // Find non zero coef
    int non_zero_coef_idx = -1;
    for(int i = 0; i < 3; i++){
        if(plane[i] != 0){
            non_zero_coef_idx = i;
        }
    }
    assert(non_zero_coef_idx != -1 && "Invalid plane");
    point_on_plane[non_zero_coef_idx] = -plane.w()/plane[non_zero_coef_idx];
    return point_on_plane;
}

Eigen::Vector4f getPlane(Eigen::Vector3f p1, Eigen::Vector3f p2, Eigen::Vector3f p3){
    Eigen::Vector3f normal = (p2 - p1).cross(p3 - p1);
    float d = -(normal.dot(p1));
    Eigen::Vector4f plane; plane << normal.x(), normal.y(), normal.z(), d;
    return plane;
}

inline Eigen::Vector3f proj(Eigen::Vector3f p, Eigen::Vector3f a, Eigen::Vector3f b){
    Eigen::Vector3f v1 = p-a;
    Eigen::Vector3f v2 = b-a;
    return (v1.dot(v2)/v2.norm())*v2;
}

inline float cross(Eigen::Vector2f a, Eigen::Vector2f b){
    return a.x()*b.y() - a.y*b.x();
}

inline bool pointInTriangle(Eigen::Vector2f p, Eigen::Vector2f a, Eigen::Vector2f b, Eigen::Vector2f c){
    bool sign1 = cross(p-a, b-a) < 0;
    bool sign2 = cross(p-b, c-b) < 0;
    bool sign3 = cross(p-c, a-c) < 0;
    return sign1 == sign2 == sign3;
}

inline Eigen::Vector2f closestCoord(Eigen::Vector2f p, Eigen::Vector2f a, Eigen::Vector2f b, Eigen::Vector2f c){
    vector<Vector2f> points = {a,b,c};
    Eigen::Vector2f minCoord;
    float min = FLT_MAX;
    for(int i = 0; i < 3; i++){
        i2 = (i+1)%3;
        Vector2f v1 = p-points[i2];
        Vector2f v2 = points[i] - points[i2];
        //project v1 onto v2
        float t = v1.dot(v2)/(v2.dot(v2));

        if(t < 0)
            t = 0;
        if(t > 1)
            t = 1;

        Vector2f p2 = points[i] + v2*t;

        float dist = (p2 - p).norm();
        if(dist < min){
            min = dist;
            minCoord = p2;
        }

    }

    assert(min != FLT_MAX);

    return minCoord;
}

float distToPlane(Eigen::Vector3f point, Eigen::Vector3f p1, Eigen::Vector3f p2, Eigen::Vector3f p3){
    //Eigen::Vector4f plane = getPlane(p1, p2, p3);

    // Calculate rotation quarterion
    Eigen::Vector3f triangle_normal = ((p2 - p1).cross(p3 - p1)).normalized();
    Eigen::Vector3f z_axis(0,0,1);
    float angle = -acos(triangle_normal.dot(z_axis));
    Eigen::Vector3f axis = triangle_normal.cross(z_axis).normalized();
    Quaternion<float> q; q = AngleAxis<float>(angle, axis);

    // rotate to xy plane
    point*=q; p1*=q; p2*=q;p3*=q;

    // make 2d
    Eigen::Vector2f point_ = point.head<2>();
    Eigen::Vector2f p1_ = p1.head<2>();
    Eigen::Vector2f p2_ = p2.head<2>();
    Eigen::Vector2f p3_ = p3.head<2>();

    // if in triangle
    bool in_triangle = pointInTriangle(point_, p1_, p2_, p3_);
    if(in_triangle){
        return std::fabs(point.z - p1.z);
    }

    // if not in triangle find the closest point the line
    Eigen::Vector2f coord = closestCoord(point_, p1_, p2_, p3_);
    Eigen::Vector3f cpoint(coord.x(), coord.y(), p1.z());

    return (point-cpoint).norm();
}

// Finds and returns the distance to the closest plane defined by a line and
// the camera origin
float MinCut::closestPolyLineDist(Eigen::Vector3f & point) const{
    float min = FLT_MAX;

    for(int i = 0; i < polygon_.size(); i++){
        int p1 = i, p2 = (i+1)%polygon_.size();
        float dist = distToPlane(point, polygon_[p1], polygon_[p2], cam_origin_);
        if(dist < min){
            min = dist;
        }
    }

    return min;
}

float MinCut::distFromCenter(Eigen::Vector3f & point) const{
    return sqrt(pow(subcloud_center_.x()-point.x(), 2) + pow(subcloud_center_.y()-point.y(), 2));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 void
MinCut::calculateUnaryPotential (int point, double& source_weight, double& sink_weight) const
{
     // Source weight constant

     // Penalize point close to the polygon boundry

     Eigen::Vector3f p;
     p << input_->points[point].x,
             input_->points[point].y,
             input_->points[point].z;

     // Apply background penalty
     double dist_to_center = distFromCenter(p);
     double dist_to_boudry = closestPolyLineDist(p);

     sink_weight = dist_to_center/(dist_to_center+dist_to_boudry);

     // Apply forground penalty
     source_weight = source_weight_;


/*
  // Given an abritrary point in the cloud.
  double min_dist_to_foreground = std::numeric_limits<double>::max ();
  double closest_foreground_point[2];
  closest_foreground_point[0] = closest_foreground_point[1] = 0; // initial closest point is the first point?

  double initial_point[] = {0.0, 0.0};
  initial_point[0] = input_->points[point].x;
  initial_point[1] = input_->points[point].y;

  // Finding the closest foreground point to it
  for (size_t i_point = 0; i_point < foreground_points_.size (); i_point++)
  {
    double dist = 0.0;
    dist += (foreground_points_[i_point].x - initial_point[0]) * (foreground_points_[i_point].x - initial_point[0]);
    dist += (foreground_points_[i_point].y - initial_point[1]) * (foreground_points_[i_point].y - initial_point[1]);

    if (min_dist_to_foreground > dist)
    {
      min_dist_to_foreground = dist;
      closest_foreground_point[0] = foreground_points_[i_point].x;
      closest_foreground_point[1] = foreground_points_[i_point].y;
    }
  }


  // Apply background penalty
  sink_weight = pow (min_dist_to_foreground / radius_, 0.5);

  // Apply forground penalty
  source_weight = source_weight_;

 */

  return;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 bool
MinCut::addEdge (int source, int target, double weight)
{
  std::set<int>::iterator iter_out = edge_marker_[source].find (target);
  if ( iter_out != edge_marker_[source].end () )
    return (false);

  EdgeDescriptor edge;
  EdgeDescriptor reverse_edge;
  bool edge_was_added, reverse_edge_was_added;

  boost::tie (edge, edge_was_added) = boost::add_edge ( vertices_[source], vertices_[target], *graph_ );
  boost::tie (reverse_edge, reverse_edge_was_added) = boost::add_edge ( vertices_[target], vertices_[source], *graph_ );
  if ( !edge_was_added || !reverse_edge_was_added )
    return (false);

  (*capacity_)[edge] = weight;
  (*capacity_)[reverse_edge] = 0.0;
  (*reverse_edges_)[edge] = reverse_edge;
  (*reverse_edges_)[reverse_edge] = edge;
  edge_marker_[source].insert (target);

  return (true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 double
MinCut::calculateBinaryPotential (int source, int target) const
{
  double weight = 0.0;
  double distance = 0.0;
  distance += (input_->points[source].x - input_->points[target].x) * (input_->points[source].x - input_->points[target].x);
  distance += (input_->points[source].y - input_->points[target].y) * (input_->points[source].y - input_->points[target].y);
  distance += (input_->points[source].z - input_->points[target].z) * (input_->points[source].z - input_->points[target].z);
  //qDebug("Dist( %f )", distance);
  distance *= inverse_sigma_;
  //qDebug("Dist*inv_sigma( %f ), inv_sigma = %f", distance, inverse_sigma_);
  weight = exp (-distance);
  //qDebug("weight( %.9f )",  weight);

  //qDebug("Source( %f, %f, %f ) @ %d, Target( %f, %f, %f ) @ %d", input_->points[source].x, input_->points[source].y, input_->points[source].z, source,
  //       input_->points[target].x, input_->points[target].y, input_->points[target].z, target);


  return (weight);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 bool
MinCut::recalculateUnaryPotentials ()
{
  OutEdgeIterator src_edge_iter;
  OutEdgeIterator src_edge_end;
  std::pair<EdgeDescriptor, bool> sink_edge;

  // Itterate though all the edges
  // Set weights to the sink. Set weights on other edges
  // Seems like every node is connected to the source
  for (boost::tie (src_edge_iter, src_edge_end) = boost::out_edges (source_, *graph_); src_edge_iter != src_edge_end; src_edge_iter++)
  {
    double source_weight = 0.0;
    double sink_weight = 0.0;
    sink_edge.second = false;
    calculateUnaryPotential (static_cast<int> (boost::target (*src_edge_iter, *graph_)), source_weight, sink_weight);
    // Lookup the edge from the current edge target to the sink
    sink_edge = boost::lookup_edge (boost::target (*src_edge_iter, *graph_), sink_, *graph_);
    // does this edge does not exist?
    if (!sink_edge.second)
      return (false);

    // Set edge weights
    (*capacity_)[*src_edge_iter] = source_weight; // source to target weight
    (*capacity_)[sink_edge.first] = sink_weight; // target to sink weight
  }

  return (true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 bool
MinCut::recalculateBinaryPotentials ()
{
  int number_of_points = static_cast<int> (indices_->size ());

  VertexIterator vertex_iter;
  VertexIterator vertex_end;
  OutEdgeIterator edge_iter;
  OutEdgeIterator edge_end;

  std::vector< std::set<VertexDescriptor> > edge_marker;
  std::set<VertexDescriptor> out_edges_marker;
  edge_marker.clear ();
  edge_marker.resize (number_of_points + 2, out_edges_marker);

  for (boost::tie (vertex_iter, vertex_end) = boost::vertices (*graph_); vertex_iter != vertex_end; vertex_iter++)
  {
    VertexDescriptor source_vertex = *vertex_iter;
    if (source_vertex == source_ || source_vertex == sink_)
      continue;
    for (boost::tie (edge_iter, edge_end) = boost::out_edges (source_vertex, *graph_); edge_iter != edge_end; edge_iter++)
    {
      //If this is not the edge of the graph, but the reverse fictitious edge that is needed for the algorithm then continue
      EdgeDescriptor reverse_edge = (*reverse_edges_)[*edge_iter];
      if ((*capacity_)[reverse_edge] != 0.0)
        continue;

      //If we already changed weight for this edge then continue
      VertexDescriptor target_vertex = boost::target (*edge_iter, *graph_);
      std::set<VertexDescriptor>::iterator iter_out = edge_marker[static_cast<int> (source_vertex)].find (target_vertex);
      if ( iter_out != edge_marker[static_cast<int> (source_vertex)].end () )
        continue;

      if (target_vertex != source_ && target_vertex != sink_)
      {
        //Change weight and remember that this edges were updated
        double weight = calculateBinaryPotential (static_cast<int> (target_vertex), static_cast<int> (source_vertex));
        (*capacity_)[*edge_iter] = weight;
        edge_marker[static_cast<int> (source_vertex)].insert (target_vertex);
      }
    }
  }

  return (true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 void
MinCut::assembleLabels (ResidualCapacityMap& residual_capacity)
{
  std::vector<int> labels;
  labels.resize (input_->points.size (), 0);
  int number_of_indices = static_cast<int> (indices_->size ());
  for (int i_point = 0; i_point < number_of_indices; i_point++)
    labels[(*indices_)[i_point]] = 1;

  clusters_.clear ();

  pcl::PointIndices segment;
  clusters_.resize (2, segment);

  OutEdgeIterator edge_iter, edge_end;
  for ( boost::tie (edge_iter, edge_end) = boost::out_edges (source_, *graph_); edge_iter != edge_end; edge_iter++ )
  {
    if (labels[edge_iter->m_target] == 1)
    {
      if (residual_capacity[*edge_iter] > epsilon_)
        clusters_[1].indices.push_back (static_cast<int> (edge_iter->m_target));
      else
        clusters_[0].indices.push_back (static_cast<int> (edge_iter->m_target));
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 pcl::PointCloud<pcl::PointXYZRGB>::Ptr
MinCut::getColoredCloud ()
{
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr colored_cloud;

  if (!clusters_.empty ())
  {
    int num_of_pts_in_first_cluster = static_cast<int> (clusters_[0].indices.size ());
    int num_of_pts_in_second_cluster = static_cast<int> (clusters_[1].indices.size ());
    int number_of_points = num_of_pts_in_first_cluster + num_of_pts_in_second_cluster;
    colored_cloud = (new pcl::PointCloud<pcl::PointXYZRGB>)->makeShared ();
    unsigned char foreground_color[3] = {255, 255, 255};
    unsigned char background_color[3] = {255, 0, 0};
    colored_cloud->width = number_of_points;
    colored_cloud->height = 1;
    colored_cloud->is_dense = input_->is_dense;

    pcl::PointXYZRGB point;
    int point_index = 0;
    for (int i_point = 0; i_point < num_of_pts_in_first_cluster; i_point++)
    {
      point_index = clusters_[0].indices[i_point];
      point.x = *(input_->points[point_index].data);
      point.y = *(input_->points[point_index].data + 1);
      point.z = *(input_->points[point_index].data + 2);
      point.r = background_color[0];
      point.g = background_color[1];
      point.b = background_color[2];
      colored_cloud->points.push_back (point);
    }

    for (int i_point = 0; i_point < num_of_pts_in_second_cluster; i_point++)
    {
      point_index = clusters_[1].indices[i_point];
      point.x = *(input_->points[point_index].data);
      point.y = *(input_->points[point_index].data + 1);
      point.z = *(input_->points[point_index].data + 2);
      point.r = foreground_color[0];
      point.g = foreground_color[1];
      point.b = foreground_color[2];
      colored_cloud->points.push_back (point);
    }
  }

  return (colored_cloud);
}

 bool
 MinCut::deinitCompute ()
 {
   // Reset the indices
  if (fake_indices_)
   {
     indices_.reset ();
     fake_indices_ = false;
   }
   return (true);
 }

 bool
MinCut::initCompute ()
 {
   // Check if input was set
   if (!input_)
     return (false);

   // If no point indices have been given, construct a set of indices for the entire input point cloud
   if (!indices_)
   {
     fake_indices_ = true;
     std::vector<int> *indices = new std::vector<int> (input_->width * input_->height);
     for (size_t i = 0; i < indices->size (); ++i) { (*indices)[i] = i; }
     indices_.reset (indices);
   }
   return (true);
 }

 /*MinCut::setHorisontalRadius(bool val){
     horisonal_radius_ = val;
 }*/
