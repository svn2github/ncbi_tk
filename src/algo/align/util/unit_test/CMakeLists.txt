##############################################################################
# 
#
include_directories(SYSTEM ${BOOST_INCLUDE})

# Include projects from this directory
include(CMakeLists.score_builder_unit_test.app.txt)
include(CMakeLists.align_filter_unit_test.app.txt)
include(CMakeLists.genomic_compart_unit_test.app.txt)
include(CMakeLists.blast_tabular_unit_test.app.txt)
include(CMakeLists.collection_scores_unit_test.app.txt)

