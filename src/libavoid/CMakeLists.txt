
set(libavoid_SRC
	actioninfo.cpp
	connectionpin.cpp
	connector.cpp
	connend.cpp
	geometry.cpp
	geomtypes.cpp
	graph.cpp
	hyperedge.cpp
	hyperedgeimprover.cpp
	hyperedgetree.cpp
	junction.cpp
	makepath.cpp
	mtst.cpp
	obstacle.cpp
	orthogonal.cpp
	router.cpp
	scanline.cpp
	shape.cpp
	timer.cpp
	vertices.cpp
	viscluster.cpp
	visibility.cpp
	vpsc.cpp


	# -------
	# Headers
	actioninfo.h
	assertions.h
	connectionpin.h
	connector.h
	connend.h
	debug.h
	geometry.h
	geomtypes.h
	graph.h
	hyperedge.h
	hyperedgeimprover.h
	hyperedgetree.h
	junction.h
	libavoid.h
	makepath.h
	mtst.h
	obstacle.h
	orthogonal.h
	router.h
	scanline.h
	shape.h
	timer.h
	vertices.h
	viscluster.h
	visibility.h
	vpsc.h
)

add_inkscape_lib(avoid_LIB "${libavoid_SRC}")
