#include "timepointsmerger.h"

#include "barcodecluster.hpp"
#include "centerclustermapper.h"
#include "centerclustermapperiterator.h"
#include "idgenerator.h"


#include <array>
#include <list>
#include <math.h>
#include <memory>
#include <set>
#include <vector>
using std::list;
using std::shared_ptr;
using std::set;

namespace barcodeSpace {
TimePointsMerger::TimePointsMerger(const std::shared_ptr<CenterClusterMapper>& t1_link,
                                   const std::shared_ptr<CenterClusterMapper>& t2_link,
                                   size_t k_len,
                                   size_t num_time_points,
                                   size_t filter_csize_threshold) :
    _t1_center_cluster_linker(t1_link),
    _t2_center_cluster_linker(t2_link),
    _k_len(k_len), _num_time_points(num_time_points),
    _mutator(k_len), _filter_csize_threshold(filter_csize_threshold)
{
}
void TimePointsMerger::merge() {

    std::set<int>     id_pool; // taken ids.
    // Build up the used id table, which is used in merge function
    // It is to make sure that the merged clusters have unique id each.

    // First merge those clusters that have at least one centers thar
    // are exactly the same.
    CenterClusterMapperIterator iter_1(_t1_center_cluster_linker);
    if (_t2_center_cluster_linker.get() == NULL) {
	while (iter_1.has_next()) {
           const std::shared_ptr<Cluster> c1 = iter_1.next();
           std::vector<size_t> my_timepoints = {0};
            my_timepoints.insert(my_timepoints.end(), c1->columns().begin(), c1->columns().end());
            //std::vector<size_t> my_timepoints = cl->columns();
            assert(my_timepoints.size() == _num_time_points + 1);
            //my_timepoints.push_front(0);
            c1->SetTimePointFrequency(my_timepoints);
            _result.push_back(c1);
	}		
	return;	
    }
    while (iter_1.has_next()) {
        const std::shared_ptr<Cluster> cl = iter_1.next();
        // Make sure this cluster is valid.
        assert(cl.get());
        id_pool.insert(cl->ClusterID());

        list<std::shared_ptr<Cluster>> matched_clusters_from_t2;
        // Get all clusters in t2 who share the at least on center with cl.
        for (const auto& center : _t1_center_cluster_linker->myCenter(cl->ClusterID())) {
            const std::shared_ptr<Cluster> temp_cluster = _t2_center_cluster_linker->getClusterByCenter(center);
            if (temp_cluster.get() != NULL) {
                matched_clusters_from_t2.push_back(temp_cluster);
                _t2_center_cluster_linker->removeCluster(temp_cluster->ClusterID());
            }
        }
        if (!matched_clusters_from_t2.empty()) {
            _t1_center_cluster_linker->removeCluster(cl->ClusterID());
            // Merge those matched clusters first;
            auto m_cl = matched_clusters_from_t2.front();
            matched_clusters_from_t2.pop_front();
            while (!matched_clusters_from_t2.empty()) {
                m_cl->merge(matched_clusters_from_t2.front());
                matched_clusters_from_t2.pop_front();
            }
            // Combine with the merged big cluster.
            cl->combine(m_cl);
            _result.push_back(cl);
        }
    }

    iter_1.reset();
    // Merge those clusters that have at least one centers that are
    // one base pair away.
    std::vector<std::string> mutated_center;
    while(iter_1.has_next()) {
    //for (const auto& cl : unmatched_cluster_t1) {
        const std::shared_ptr<Cluster> cl = iter_1.next();
        assert(cl.get());
	mutated_center.clear();
        _mutator.mutateCenterInplace(cl->center(), mutated_center);
        double ratio = 0;
        double sz = cl->size();
        std::shared_ptr<Cluster> matched_cluster_from_t2;
        for (const auto& center : mutated_center) {
            if (_t2_center_cluster_linker->hasCenter(center)) {
                /*
                matched_clusters_from_t2.push_back(_t2_center_cluster_linker->getClusterByCenter(center));
                // remove the cluster immediately in case it is matched by multiple times
                _t2_center_cluster_linker->removeCluster(matched_clusters_from_t2.back()->ClusterID());*/
                double tmp_sz = _t2_center_cluster_linker->getClusterByCenter(center)->size();
                if (tmp_sz > sz) {
                    std::swap(tmp_sz, sz);
                }
                if ( sz/tmp_sz > ratio) {
                    matched_cluster_from_t2 = _t2_center_cluster_linker->getClusterByCenter(center);
                }
                sz = cl->size();
            }
        }
        _t1_center_cluster_linker->removeCluster(cl->ClusterID());
        if (matched_cluster_from_t2.get()) {
            cl->combine(matched_cluster_from_t2);
            _t2_center_cluster_linker->removeCluster(matched_cluster_from_t2->ClusterID());
            _result.push_back(cl);
        } else if (cl->size() >= _filter_csize_threshold){
            // add dummy time point to those unmatched high
            // frequent clusters to t1 time points.
	    std::vector<size_t> my_timepoints = {0};
	    my_timepoints.insert(my_timepoints.end(), cl->columns().begin(), cl->columns().end());
            assert(my_timepoints.size() == _num_time_points + 1);
            cl->SetTimePointFrequency(my_timepoints);
            _result.push_back(cl);
        }
    }
    // Extract all unmatched clusters for time point t2.
    CenterClusterMapperIterator iter_2(_t2_center_cluster_linker);
    std::unique_ptr<IDGenerator>    id_generator(new IDGenerator(id_pool));
    assert(id_generator.get());
    while (iter_2.has_next()) {
        const shared_ptr<Cluster> cl = iter_2.next();
        // add previous dummy time points to make the cluster
        if (_num_time_points > 0) {
            std::vector<freq> dummy_time_points(_num_time_points, 0);
            const std::vector<freq>& my_freq = cl->columns();
            dummy_time_points.insert(dummy_time_points.begin(), my_freq.begin(), my_freq.end());
            cl->SetTimePointFrequency(dummy_time_points);
        }
        _result.push_back(cl);
        cl->SetClusterID(id_generator->nextID());
    }
}
}
