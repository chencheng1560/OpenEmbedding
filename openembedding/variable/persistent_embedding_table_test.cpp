#include <gtest/gtest.h>
#include "PersistentEmbeddingTable.h"
#include <limits>

namespace paradigm4 {
namespace pico {
namespace embedding {

TEST(PersistentEmbeddingTable, MultipleGetAndSet) {
    PersistentManager::singleton().set_cache_size(10);
    PersistentEmbeddingTable<uint64_t,double> pt(64, -1);
    core::Configure config;
    PersistentManager::singleton().set_pmem_pool_root_path("/mnt/pmem0/test");
    std::string pmem_pool_path = PersistentManager::singleton().new_pmem_pool_path();
    SAVE_CONFIG(config, pmem_pool_path);
    pt.load_config(config);

    size_t total_items = 5;
    
    double* tmp;
    for(size_t j=0; j<total_items; ++j){
        EXPECT_EQ(j, pt.batch_id());
        EXPECT_EQ(nullptr, pt.get_value(j));
        tmp = pt.set_value(j);
        for(size_t i=0; i<64; ++i){
            *tmp = double(i+j);
            ++tmp;
        }
        tmp = (double *)pt.get_value(j);
        for(size_t i=0; i<64; ++i){
            EXPECT_EQ(double(i+j), *tmp);
            ++tmp;
        }
        pt.next_batch();
    }
    EXPECT_EQ(total_items, pt.batch_id());
    
    for(size_t k=0; k<total_items; ++k){
        std::cout<<"k:"<<k<<std::endl;
        tmp = (double *)pt.get_value(k);
        for(size_t i=0; i<64; ++i){
            EXPECT_EQ(double(i+k), *tmp);
            ++tmp;
        }
    }
}

TEST(PersistentEmbeddingTable, SingleCheckpoint) {  
    PersistentManager::singleton().set_cache_size(5*64*sizeof(double));  //??
    PersistentEmbeddingTable<uint64_t,double> pt(64, -1);
    core::Configure config;
    PersistentManager::singleton().set_pmem_pool_root_path("/mnt/pmem0/test");
    config.node()["pmem_pool_path"] = PersistentManager::singleton().new_pmem_pool_path();
// initial status    
    double* tmp;
    EXPECT_EQ(0, pt.batch_id());
    EXPECT_EQ(0, pt.checkpoints().size());
    EXPECT_EQ(0, pt.get_pmem_vector_size());
    EXPECT_EQ(0, pt.get_avaiable_freespace_slots());
    EXPECT_EQ(0, pt.get_all_freespace_slots());
//////
// exp1: set 0,1,2,3,4 at each batch
    for(size_t j=0; j<5; ++j){
        EXPECT_EQ(j, pt.batch_id());
        EXPECT_EQ(nullptr, pt.get_value(j));
        tmp = pt.set_value(j);
        for(size_t i=0; i<64; ++i){
            *tmp = double(i+j);
            ++tmp;
        }
        tmp = (double *)pt.get_value(j);
        for(size_t i=0; i<64; ++i){
            EXPECT_EQ(double(i+j), *tmp);
            ++tmp;
        }
        pt.next_batch();
    }
    EXPECT_EQ(5, pt.batch_id());
    pt.start_commit_checkpoint();  //_committing=5
    //status 1 expect: 
    // deque checkpoints: null
    // _committing = 5
    // _batch_id = 5
    // _free_list: null

    // Content: (dram,batch_id,key,value):
    // dram,0,0,0-63;
    // dram,1,1,1-64;
    // dram,2,2,2-65;
    // dram,3,3,3-66;
    // dram,4,4,4-67;
    EXPECT_EQ(0, pt.checkpoints().size());
    EXPECT_EQ(0, pt.get_pmem_vector_size());
    EXPECT_EQ(0, pt.get_avaiable_freespace_slots());
    EXPECT_EQ(0, pt.get_all_freespace_slots());
//////
// exp2: reset key 0,1,2,3,4's value at batch 5
    //test 1, set 0 at batch 5
    tmp = pt.set_value(0);
    for(size_t i=0; i<64; ++i){
        *tmp = (*tmp) + 10;
        ++tmp;
    }
    EXPECT_EQ(5, pt.batch_id());
    EXPECT_EQ(0, pt.checkpoints().size());
    EXPECT_EQ(1, pt.get_pmem_vector_size());
    EXPECT_EQ(0, pt.get_avaiable_freespace_slots());
    EXPECT_EQ(1, pt.get_all_freespace_slots());
    //test 2, set 
    for(int k=1; k<5; ++k){
        tmp = pt.set_value(k);
        for(size_t i=0; i<64; ++i){
            *tmp = (*tmp) + 10;
            ++tmp;
        }
    }
    //status 2 expect: 
    // deque checkpoints: null
    // _committing = 5
    // _batch_id = 5
    // _free_list: null

    // Content: (dram,batch_id,key,value):
    // dram,5,0,10-73;
    // dram,5,1,11-74;
    // dram,5,2,12-75;
    // dram,5,3,13-76;
    // dram,5,4,14-77;
//////
// exp3: reset 0,1,2,3,4 at batch 6
    pt.next_batch();
    EXPECT_EQ(6, pt.batch_id());
    EXPECT_EQ(1, pt.checkpoints().size());
    EXPECT_EQ(5, pt.get_pmem_vector_size());
    EXPECT_EQ(0, pt.get_avaiable_freespace_slots());
    EXPECT_EQ(5, pt.get_all_freespace_slots());
    //test 1, set 0 at batch 6
    tmp = pt.set_value(0);
    for(size_t i=0; i<64; ++i){
        *tmp = (*tmp) + 10;
        ++tmp;
    }
    pt.next_batch();
    EXPECT_EQ(7, pt.batch_id());
    EXPECT_EQ(1, pt.checkpoints().size());
    EXPECT_EQ(5, pt.get_pmem_vector_size());
    EXPECT_EQ(0, pt.get_avaiable_freespace_slots());
    EXPECT_EQ(5, pt.get_all_freespace_slots());
    
    pt.start_commit_checkpoint(); //_committing=7
    //test 1, set 0 at batch 6
    for(size_t k=0; k<100; ++k){
        tmp = pt.set_value(0);
        for(size_t i=0; i<64; ++i){
            *tmp = (*tmp) + 10;
            ++tmp;
        }
        //pt.next_batch();
    }
    pt.next_batch();
    EXPECT_EQ(8, pt.batch_id());
    EXPECT_EQ(1, pt.checkpoints().size());
    EXPECT_EQ(6, pt.get_pmem_vector_size());
    EXPECT_EQ(0, pt.get_avaiable_freespace_slots());
    EXPECT_EQ(6, pt.get_all_freespace_slots()); 
    //_free_space  batch_id=0 key=0,1,2,3,4; batch_id=1 key=0

    //test
    for(int i=1, i<6;++i){
        tmp = pt.set_value(0);
        for(size_t i=0; i<64; ++i){
            *tmp = (*tmp) + 10;
            ++tmp;
        }
        pt.next_batch();
    }
    EXPECT_EQ(13, pt.batch_id());
    EXPECT_EQ(2, pt.checkpoints().size());
    EXPECT_EQ(10, pt.get_pmem_vector_size());
    EXPECT_EQ(0, pt.get_avaiable_freespace_slots());
    EXPECT_EQ(11, pt.get_all_freespace_slots());
    //_free_space  batch_id=0 key=0,1,2,3,4; batch_id=1 key=0; batch_id=2 key=0,1,2,3,4
    if(pt.checkpoints().size()>=2){
        pt.pop_checkpoint();
    }
    pt.next_batch();
    EXPECT_EQ(14, pt.batch_id());
    EXPECT_EQ(1, pt.checkpoints().size());
    EXPECT_EQ(10, pt.get_pmem_vector_size());
    EXPECT_EQ(5, pt.get_avaiable_freespace_slots());
    EXPECT_EQ(11, pt.get_all_freespace_slots());
    
///TODO:继续其他各种case
}



}
}
}

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
