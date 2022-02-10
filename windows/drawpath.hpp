// 7 september 2015
#include "uipriv_windows.hpp"
#include "draw.hpp"

struct uiDrawPath {
	ID2D1PathGeometry *path;
	ID2D1TransformedGeometry *transformedPath;
	ID2D1GeometrySink *sink;
	D2D1_FILL_MODE fillMode; // cause ID2D1GeometrySink->SetFillMode exists wheras ->GetFillMode does not
	BOOL inFigure;
};
