##############################################################################
# 
#

include_directories(SYSTEM ${BOOST_INCLUDE})

# Include projects from this directory
include(CMakeLists.unit_test_idmapper.app.txt)
include(CMakeLists.unit_test_format_guess.app.txt)
include(CMakeLists.unit_test_format_guess_ex.app.txt)
include(CMakeLists.unit_test_feature_table_reader.app.txt)
include(CMakeLists.unit_test_fasta_reader.app.txt)
include(CMakeLists.unit_test_agp_seq_entry.app.txt)
include(CMakeLists.unit_test_agp_converter.app.txt)
include(CMakeLists.unit_test_gvfreader.app.txt)
include(CMakeLists.unit_test_gff3reader.app.txt)
include(CMakeLists.unit_test_bedreader.app.txt)
include(CMakeLists.unit_test_wigreader.app.txt)
include(CMakeLists.unit_test_vcfreader.app.txt)
include(CMakeLists.unit_test_ucscreader.app.txt)
include(CMakeLists.unit_test_gtfreader.app.txt)

