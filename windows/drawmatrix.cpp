// 7 september 2015
#include "drawpath.hpp"

void m2d(uiDrawMatrix *m, D2D1_MATRIX_3X2_F *d)
{
	d->_11 = m->M11;
	d->_12 = m->M12;
	d->_21 = m->M21;
	d->_22 = m->M22;
	d->_31 = m->M31;
	d->_32 = m->M32;
}

static void d2m(D2D1_MATRIX_3X2_F *d, uiDrawMatrix *m)
{
	m->M11 = d->_11;
	m->M12 = d->_12;
	m->M21 = d->_21;
	m->M22 = d->_22;
	m->M31 = d->_31;
	m->M32 = d->_32;
}

// Mostly copied from uiDrawNewPath
uiDrawPath *uiDrawPathCopyByTransform(uiDrawPath *p, uiDrawMatrix *m)
{
	D2D1_MATRIX_3X2_F dm;
	uiDrawPath *r;
	HRESULT hr;

	m2d(m, &dm);

	r = uiprivNew(uiDrawPath);
	hr = d2dfactory->CreateTransformedGeometry(
		p->path,
		&dm,
		&(r->transformedPath)
	);
	// Note there are two versions (possibly depending on Windows version):
	// https://docs.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1factory-createtransformedgeometry(id2d1geometry_constd2d1_matrix_3x2_f_id2d1transformedgeometry)
	// https://docs.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1factory-createtransformedgeometry(id2d1geometry_constd2d1_matrix_3x2_f__id2d1transformedgeometry)
	if (hr != S_OK)
		logHRESULT(L"error creating path", hr);
	hr = r->path->Open(&(r->sink));
	if (hr != S_OK)
		logHRESULT(L"error opening path", hr);
	r->sink->SetFillMode(p->fillMode);
	// Written blindly
	if (p->sink == NULL)
		uiDrawPathEnd(r);
	return r;
}

void uiDrawMatrixTranslate(uiDrawMatrix *m, double x, double y)
{
	D2D1_MATRIX_3X2_F dm;

	m2d(m, &dm);
	dm = dm * D2D1::Matrix3x2F::Translation(x, y);
	d2m(&dm, m);
}

void uiDrawMatrixScale(uiDrawMatrix *m, double xCenter, double yCenter, double x, double y)
{
	D2D1_MATRIX_3X2_F dm;
	D2D1_POINT_2F center;

	m2d(m, &dm);
	center.x = xCenter;
	center.y = yCenter;
	dm = dm * D2D1::Matrix3x2F::Scale(x, y, center);
	d2m(&dm, m);
}

#define r2d(x) (x * (180.0 / uiPi))

void uiDrawMatrixRotate(uiDrawMatrix *m, double x, double y, double amount)
{
	D2D1_MATRIX_3X2_F dm;
	D2D1_POINT_2F center;

	m2d(m, &dm);
	center.x = x;
	center.y = y;
	dm = dm * D2D1::Matrix3x2F::Rotation(r2d(amount), center);
	d2m(&dm, m);
}

void uiDrawMatrixSkew(uiDrawMatrix *m, double x, double y, double xamount, double yamount)
{
	D2D1_MATRIX_3X2_F dm;
	D2D1_POINT_2F center;

	m2d(m, &dm);
	center.x = x;
	center.y = y;
	dm = dm * D2D1::Matrix3x2F::Skew(r2d(xamount), r2d(yamount), center);
	d2m(&dm, m);
}

void uiDrawMatrixMultiply(uiDrawMatrix *dest, uiDrawMatrix *src)
{
	D2D1_MATRIX_3X2_F c, d;

	m2d(dest, &c);
	m2d(src, &d);
	c = c * d;
	d2m(&c, dest);
}

int uiDrawMatrixInvertible(uiDrawMatrix *m)
{
	D2D1_MATRIX_3X2_F d;

	m2d(m, &d);
	return D2D1IsMatrixInvertible(&d) != FALSE;
}

int uiDrawMatrixInvert(uiDrawMatrix *m)
{
	D2D1_MATRIX_3X2_F d;

	m2d(m, &d);
	if (D2D1InvertMatrix(&d) == FALSE)
		return 0;
	d2m(&d, m);
	return 1;
}

void uiDrawMatrixTransformPoint(uiDrawMatrix *m, double *x, double *y)
{
	D2D1::Matrix3x2F dm;
	D2D1_POINT_2F pt;

	m2d(m, &dm);
	pt.x = *x;
	pt.y = *y;
	pt = dm.TransformPoint(pt);
	*x = pt.x;
	*y = pt.y;
}

void uiDrawMatrixTransformSize(uiDrawMatrix *m, double *x, double *y)
{
	uiprivFallbackTransformSize(m, x, y);
}
