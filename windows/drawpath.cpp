// 7 september 2015
#include "drawpath.hpp"

// TODO
// - write a test for transform followed by clip and clip followed by transform to make sure they work the same as on gtk+ and cocoa
// - write a test for nested transforms for gtk+

uiDrawPath *uiDrawNewPath(uiDrawFillMode fillmode)
{
	uiDrawPath *p;
	HRESULT hr;

	p = uiprivNew(uiDrawPath);
	hr = d2dfactory->CreatePathGeometry(&(p->path));
	if (hr != S_OK)
		logHRESULT(L"error creating path", hr);
	hr = p->path->Open(&(p->sink));
	if (hr != S_OK)
		logHRESULT(L"error opening path", hr);
	switch (fillmode) {
	case uiDrawFillModeWinding:
		p->fillMode = D2D1_FILL_MODE_WINDING;
		p->sink->SetFillMode(D2D1_FILL_MODE_WINDING);
		break;
	case uiDrawFillModeAlternate:
		p->fillMode = D2D1_FILL_MODE_ALTERNATE;
		p->sink->SetFillMode(D2D1_FILL_MODE_ALTERNATE);
		break;
	}
	return p;
}

void uiDrawFreePath(uiDrawPath *p)
{
	if (p->inFigure)
		p->sink->EndFigure(D2D1_FIGURE_END_OPEN);
	if (p->sink != NULL)
		// TODO close sink first?
		p->sink->Release();
	if (p->path)
		p->path->Release();
	if (p->transformedPath)
		p->transformedPath->Release();
	uiprivFree(p);
}

void uiDrawPathNewFigure(uiDrawPath *p, double x, double y)
{
	D2D1_POINT_2F pt;

	if (p->inFigure)
		p->sink->EndFigure(D2D1_FIGURE_END_OPEN);
	pt.x = x;
	pt.y = y;
	p->sink->BeginFigure(pt, D2D1_FIGURE_BEGIN_FILLED);
	p->inFigure = TRUE;
}

// Direct2D arcs require a little explanation.
// An arc in Direct2D is defined by the chord between the endpoints.
// There are four possible arcs with the same two endpoints that you can draw this way.
// See https://www.youtube.com/watch?v=ATS0ANW1UxQ for a demonstration.
// There is a property rotationAngle which deals with the rotation /of the entire ellipse that forms an ellpitical arc/ - it's effectively a transformation on the arc.
// That is to say, it's NOT THE SWEEP.
// The sweep is defined by the start and end points and whether the arc is "large".
// As a result, this design does not allow for full circles or ellipses with a single arc; they have to be simulated with two.
// TODO https://github.com/Microsoft/WinObjC/blob/develop/Frameworks/CoreGraphics/CGPath.mm#L313

struct arc {
	double xCenter;
	double yCenter;
	double radius;
	double startAngle;
	double sweep;
	int negative;
};

// this is used for the comparison below
// if it falls apart it can be changed later
#define aerMax 6 * DBL_EPSILON

static void drawArc(uiDrawPath *p, struct arc *a, void (*startFunction)(uiDrawPath *, double, double))
{
	double sinX, cosX;
	double startX, startY;
	double endX, endY;
	D2D1_ARC_SEGMENT as;
	BOOL fullCircle;
	double absSweep;

	// as above, we can't do a full circle with one arc
	// simulate it with two half-circles
	// of course, we have a dragon: equality on floating-point values!
	// I've chosen to do the AlmostEqualRelative() technique in https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
	fullCircle = FALSE;
	// use the absolute value to tackle both ≥2π and ≤-2π at the same time
	absSweep = fabs(a->sweep);
	if (absSweep > (2 * uiPi))		// this part is easy
		fullCircle = TRUE;
	else {
		double aerDiff;

		aerDiff = fabs(absSweep - (2 * uiPi));
		// if we got here then we know a->sweep is larger (or the same!)
		fullCircle = aerDiff <= absSweep * aerMax;
	}
	// TODO make sure this works right for the negative direction
	if (fullCircle) {
		a->sweep = uiPi;
		drawArc(p, a, startFunction);
		a->startAngle += uiPi;
		drawArc(p, a, NULL);
		return;
	}

	// first, figure out the arc's endpoints
	// unfortunately D2D1SinCos() is only defined on Windows 8 and newer
	// the MSDN page doesn't say this, but says it requires d2d1_1.h, which is listed as only supported on Windows 8 and newer elsewhere on MSDN
	// so we must use sin() and cos() and hope it's right...
	sinX = sin(a->startAngle);
	cosX = cos(a->startAngle);
	startX = a->xCenter + a->radius * cosX;
	startY = a->yCenter + a->radius * sinX;
	sinX = sin(a->startAngle + a->sweep);
	cosX = cos(a->startAngle + a->sweep);
	endX = a->xCenter + a->radius * cosX;
	endY = a->yCenter + a->radius * sinX;

	// now do the initial step to get the current point to be the start point
	// this is either creating a new figure, drawing a line, or (in the case of our full circle code above) doing nothing
	if (startFunction != NULL)
		(*startFunction)(p, startX, startY);

	// now we can draw the arc
	as.point.x = endX;
	as.point.y = endY;
	as.size.width = a->radius;
	as.size.height = a->radius;
	as.rotationAngle = 0;		// as above, not relevant for circles
	if (a->negative)
		as.sweepDirection = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
	else
		as.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
	// TODO explain the outer if
	if (!a->negative)
		if (a->sweep > uiPi)
			as.arcSize = D2D1_ARC_SIZE_LARGE;
		else
			as.arcSize = D2D1_ARC_SIZE_SMALL;
	else
		// TODO especially this part
		if (a->sweep > uiPi)
			as.arcSize = D2D1_ARC_SIZE_SMALL;
		else
			as.arcSize = D2D1_ARC_SIZE_LARGE;
	p->sink->AddArc(&as);
}

void uiDrawPathNewFigureWithArc(uiDrawPath *p, double xCenter, double yCenter, double radius, double startAngle, double sweep, int negative)
{
	struct arc a;

	a.xCenter = xCenter;
	a.yCenter = yCenter;
	a.radius = radius;
	a.startAngle = startAngle;
	a.sweep = sweep;
	a.negative = negative;
	drawArc(p, &a, uiDrawPathNewFigure);
}

void uiDrawPathLineTo(uiDrawPath *p, double x, double y)
{
	D2D1_POINT_2F pt;

	pt.x = x;
	pt.y = y;
	p->sink->AddLine(pt);
}

void uiDrawPathArcTo(uiDrawPath *p, double xCenter, double yCenter, double radius, double startAngle, double sweep, int negative)
{
	struct arc a;

	a.xCenter = xCenter;
	a.yCenter = yCenter;
	a.radius = radius;
	a.startAngle = startAngle;
	a.sweep = sweep;
	a.negative = negative;
	drawArc(p, &a, uiDrawPathLineTo);
}

void uiDrawPathBezierTo(uiDrawPath *p, double c1x, double c1y, double c2x, double c2y, double endX, double endY)
{
	D2D1_BEZIER_SEGMENT s;

	s.point1.x = c1x;
	s.point1.y = c1y;
	s.point2.x = c2x;
	s.point2.y = c2y;
	s.point3.x = endX;
	s.point3.y = endY;
	p->sink->AddBezier(&s);
}

void uiDrawPathCloseFigure(uiDrawPath *p)
{
	p->sink->EndFigure(D2D1_FIGURE_END_CLOSED);
	p->inFigure = FALSE;
}

void uiDrawPathAddRectangle(uiDrawPath *p, double x, double y, double width, double height)
{
	// this is the same algorithm used by cairo and Core Graphics, according to their documentations
	uiDrawPathNewFigure(p, x, y);
	uiDrawPathLineTo(p, x + width, y);
	uiDrawPathLineTo(p, x + width, y + height);
	uiDrawPathLineTo(p, x, y + height);
	uiDrawPathCloseFigure(p);
}

void uiDrawPathEnd(uiDrawPath *p)
{
	HRESULT hr;

	if (p->inFigure) {
		p->sink->EndFigure(D2D1_FIGURE_END_OPEN);
		// needed for uiDrawFreePath()
		p->inFigure = FALSE;
	}
	hr = p->sink->Close();
	if (hr != S_OK)
		logHRESULT(L"error closing path", hr);
	p->sink->Release();
	// also needed for uiDrawFreePath()
	p->sink = NULL;
}

int uiDrawPathEnded(uiDrawPath *p)
{
	return p->sink == NULL;
}

struct uiprivDrawPathForEachInfo {
	uiDrawPathForEachFunction f;
	void *arg;
	uiForEach r; // might aswell sync w/ darwin impl
};

// model https://docs.microsoft.com/en-us/windows/win32/direct2d/how-to-use-id2d1simplified-geometrysink?redirectedfrom=MSDN
class uiprivDrawPathForEachSink : public ID2D1SimplifiedGeometrySink
{
		UINT _refCount = 1;
		struct uiprivDrawPathForEachInfo _info; // = {forEach, arg, uiForEachContinue};
	public:
		uiprivDrawPathForEachSink(struct uiprivDrawPathForEachInfo info) : _info(info) {}

		STDMETHOD_(void, AddBeziers)(const D2D1_BEZIER_SEGMENT *beziers, UINT beziersCount)
		{
			for (UINT i = 0; i < beziersCount; ++i) {
				if (_info.r == uiForEachStop)
					return;
				_info.r = _info.f({
					uiDrawPathItemTypeBezierTo,
					beziers[i].point1.x, beziers[i].point1.y,
					beziers[i].point2.x, beziers[i].point2.y,
					beziers[i].point3.x, beziers[i].point3.y,
				}, _info.arg);
			}
		}

		STDMETHOD_(void, AddLines)(const D2D1_POINT_2F *points, UINT pointsCount)
		{
			for (UINT i = 0; i < pointsCount; ++i) {
				if (_info.r == uiForEachStop)
					return;
				_info.r = _info.f({
					uiDrawPathItemTypeLineTo,
					points[i].x, points[i].y
				}, _info.arg);
			}
		}

		STDMETHOD_(void, BeginFigure)(D2D1_POINT_2F point, D2D1_FIGURE_BEGIN figureBegin)
		{
			/* typedef enum D2D1_FIGURE_BEGIN {
				D2D1_FIGURE_BEGIN_FILLED = 0,
				D2D1_FIGURE_BEGIN_HOLLOW = 1,
				D2D1_FIGURE_BEGIN_FORCE_DWORD = 0xffffffff
			} */
			if (_info.r == uiForEachStop)
				return;
			_info.r = _info.f({
				uiDrawPathItemTypeNewFigure,
				point.x, point.y
			}, _info.arg);
		}

		STDMETHOD_(void, EndFigure)(D2D1_FIGURE_END figureEnd)
		{
			//uiDrawPathItemTypeEnd, // https://docs.microsoft.com/en-us/windows/win32/api/d2d1/ne-d2d1-d2d1_figure_end
			/* typedef enum D2D1_FIGURE_END {
				D2D1_FIGURE_END_OPEN = 0,
				D2D1_FIGURE_END_CLOSED = 1,
				D2D1_FIGURE_END_FORCE_DWORD = 0xffffffff
			} */
		}

		STDMETHOD_(void, SetFillMode)(D2D1_FILL_MODE fillMode)
		{
			//uiDrawPathItemTypeFillMode // item data tbd
			/* typedef enum D2D1_FILL_MODE {
				D2D1_FILL_MODE_ALTERNATE = 0,
				D2D1_FILL_MODE_WINDING = 1,
				D2D1_FILL_MODE_FORCE_DWORD = 0xffffffff
			} */
		}
		STDMETHOD_(void, SetSegmentFlags)(D2D1_PATH_SEGMENT vertexFlags)
		{
			//uiDrawPathItemTypeSegmentFlags // item data tbd
			/* typedef enum D2D1_PATH_SEGMENT {
				D2D1_PATH_SEGMENT_NONE = 0x00000000,
				D2D1_PATH_SEGMENT_FORCE_UNSTROKED = 0x00000001,
				D2D1_PATH_SEGMENT_FORCE_ROUND_LINE_JOIN = 0x00000002,
				D2D1_PATH_SEGMENT_FORCE_DWORD = 0xffffffff
			} */
		}

		STDMETHOD(Close)()
		{
			//uiDrawPathItemTypeCloseFigure
			return S_OK;
		}

		STDMETHOD_(ULONG, Release)(THIS)
		{
			ULONG cRef = static_cast<ULONG>(
			InterlockedDecrement(reinterpret_cast<LONG volatile *>(&_refCount)));
			if(0 == cRef)
				delete this;
			return cRef;
		}
		STDMETHOD(QueryInterface)(THIS_ REFIID iid, void** ppvObject)
		{
			HRESULT hr = S_OK;
			if (__uuidof(IUnknown) == iid) {
				*ppvObject = static_cast<IUnknown*>(this);
				AddRef();
			} else if (__uuidof(ID2D1SimplifiedGeometrySink) == iid) {
				*ppvObject = static_cast<ID2D1SimplifiedGeometrySink*>(this);
				AddRef();
			} else {
				*ppvObject = NULL;
				hr = E_NOINTERFACE;
			}
			return hr;
		}
		STDMETHOD_(ULONG, AddRef)(THIS)
		{
			return InterlockedIncrement(reinterpret_cast<LONG volatile *>(&_refCount));
		}
};

void uiDrawPathForEach(uiDrawPath *p, uiDrawPathForEachFunction forEach, void *arg)
{
	struct uiprivDrawPathForEachInfo info = {forEach, arg, uiForEachContinue};

	HRESULT hr;
	uiprivDrawPathForEachSink *s = new uiprivDrawPathForEachSink(info);
	if (!s)
		hr = E_OUTOFMEMORY;
	else {
		// calling g->Simplify on common super-interface ID2D1Geometry *g assigned to ternary result (of p->transformedPath or p->path, depending on former) yields error C2446: ':': no conversion … and requires cl /Zc:ternary option, but possibly also c++17, and until verified/fixed in chain I'll do an if-else instead https://docs.microsoft.com/en-us/cpp/build/reference/zc-ternary
		if (p->transformedPath) {
			hr = p->transformedPath->Simplify(
				D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES, // D2D1_GEOMETRY_SIMPLIFICATION_OPTION_LINES, // latter flattens
				NULL, // D2D1_MATRIX_3X2_F [&] worldTransform
				// (FLOAT)flatteningTolerance,
				s
			);
		} else {
			hr = p->path->Simplify(
				D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES, // D2D1_GEOMETRY_SIMPLIFICATION_OPTION_LINES, // latter flattens
				NULL, // D2D1_MATRIX_3X2_F [&] worldTransform
				// (FLOAT)flatteningTolerance,
				s
			);
		}

		if (SUCCEEDED(hr))
			hr = s->Close();

		s->Release();
	}
}

// see drawmatrix.cpp for uiDrawPathCopyByTransform

ID2D1PathGeometry *pathGeometry(uiDrawPath *p)
{
	if (p->sink != NULL)
		uiprivUserBug("You cannot draw with a uiDrawPath that was not ended. (path: %p)", p);
	return p->path;
}

ID2D1TransformedGeometry *transformedPathGeometry(uiDrawPath *p)
{
	if (p->sink != NULL)
		uiprivUserBug("You cannot draw with a uiDrawPath that was not ended. (path: %p)", p);
	return p->transformedPath;
}
