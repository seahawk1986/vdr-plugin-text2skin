//								-*- c++ -*-

#include "screen.h"
#include <algorithm>
#include <cstring>

cText2SkinScreen::cText2SkinScreen(bool OffScreen):
	mOsd(OffScreen ? NULL : cOsdProvider::NewOsd(0, 0)),
	mScreen(OffScreen ? new cBitmap(720, 576, 8) : NULL),
	mOffScreen(OffScreen),
	mNumRegions(0),
	mCanvas(NULL),
	mOrigin(0, 0)
{
}

cText2SkinScreen::~cText2SkinScreen()
{
	for (int i = 0; i < mNumRegions; ++i)
		delete mRegions[i];
	delete mCanvas;
	delete mScreen;
	delete mOsd;
}

eOsdError cText2SkinScreen::SetAreas(const tArea *Areas, int NumAreas)
{
	if (NumAreas < 1 || NumAreas > MAXOSDAREAS)
		return oeTooManyAreas;

	// A skin asks for true colour with bpp="32" on all windows of a display.
	// VDR handles true colour as a single area, so it covers all windows.
	bool trueColor = !mOffScreen;
	for (int i = 0; i < NumAreas; ++i)
		if (Areas[i].bpp != 32)
			trueColor = false;

	if (trueColor) {
		tArea area = Areas[0];
		for (int i = 1; i < NumAreas; ++i) {
			area.x1 = std::min(area.x1, Areas[i].x1);
			area.y1 = std::min(area.y1, Areas[i].y1);
			area.x2 = std::max(area.x2, Areas[i].x2);
			area.y2 = std::max(area.y2, Areas[i].y2);
		}
		area.bpp = 32;
		if (mOsd->CanHandleAreas(&area, 1) == oeOk && mOsd->SetAreas(&area, 1) == oeOk
		    && mOsd->IsTrueColor()) {
			mOrigin = cPoint(area.x1, area.y1);
			mCanvas = new cText2SkinCanvas(cSize(area.Width(), area.Height()));
			Clear();
			return oeOk;
		}
		isyslog("text2skin: true colour OSD not available, using 256 colours");
	}

	// palette mode, windows asking for true colour get 256 colours
	tArea areas[MAXOSDAREAS];
	for (int i = 0; i < NumAreas; ++i) {
		areas[i] = Areas[i];
		if (areas[i].bpp > 8)
			areas[i].bpp = 8;
	}

	if (!mOffScreen) {
		eOsdError result = mOsd->CanHandleAreas(areas, NumAreas);
		if (result == oeOk)
			mOsd->SetAreas(areas, NumAreas);
		else
			return result;
	}

	mNumRegions = NumAreas;
	for (int i = 0; i < mNumRegions; ++i)
		mRegions[i] = new cBitmap(areas[i].Width(), areas[i].Height(), areas[i].bpp, areas[i].x1, areas[i].y1);
	Clear();

	return oeOk;
}

void cText2SkinScreen::Touch(int x1, int y1, int x2, int y2)
{
	for (int i = 0; i < mNumRegions; ++i)
		if (mRegions[i]->Intersects(x1, y1, x2, y2))
			mTouched[i] = true;
}

void cText2SkinScreen::Clear(void)
{
	if (mCanvas) {
		mCanvas->Clear();
		return;
	}
	for (int i = 0; i < mNumRegions; ++i) {
		mTouched[i] = true;
		mRegions[i]->Reset();
		mRegions[i]->Clean();
		mRegions[i]->DrawRectangle(mRegions[i]->X0(), mRegions[i]->Y0(), mRegions[i]->X0() + mRegions[i]->Width() - 1, mRegions[i]->Y0() + mRegions[i]->Height() - 1, clrTransparent);
	}
}

// Alpha blends a block of pixels into the canvas, either from Colors (ARGB)
// or from a palette Bitmap. Pixels of ColorMask are left out.
void cText2SkinScreen::BlendPixels(int x, int y, int Width, int Height, const tColor *Colors,
                                   const cBitmap *Bitmap, const tColor *ColorMask)
{
	x -= mOrigin.X();
	y -= mOrigin.Y();
	cRect r = cRect(x, y, Width, Height).Intersected(mCanvas->DrawPort().Size());
	if (r.IsEmpty())
		return;

	LOCK_PIXMAPS;
	int stride = mCanvas->DrawPort().Width();
	tColor *dst = mCanvas->Pixels();
	for (int iy = r.Top(); iy <= r.Bottom(); ++iy) {
		tColor *d = dst + iy * stride + r.Left();
		for (int ix = r.Left(); ix <= r.Right(); ++ix, ++d) {
			tColor c = Colors ? Colors[(iy - y) * Width + ix - x]
			                  : Bitmap->Color(*Bitmap->Data(ix - x, iy - y));
			if (ColorMask && c == *ColorMask)
				continue;
			uint8_t a = c >> 24;
			if (a == ALPHA_OPAQUE)
				*d = c;
			else if (a != ALPHA_TRANSPARENT)
				*d = AlphaBlend(c, *d);
		}
	}
	mCanvas->MarkDirty(r);
}

void cText2SkinScreen::DrawBitmap(int x, int y, const cBitmap &Bitmap, const tColor *ColorMask)
{
	if (mCanvas) {
		BlendPixels(x, y, Bitmap.Width(), Bitmap.Height(), NULL, &Bitmap, ColorMask);
		return;
	}
	Touch(x, y, x + Bitmap.Width() - 1, y + Bitmap.Height() - 1);
	for (int i = 0; i < mNumRegions; ++i)
		//DrawBitmapOverlay(*mRegions[i], x, y, (cBitmap&)Bitmap, ColorMask);
		mRegions[i]->DrawBitmap(x, y, Bitmap);
}

void cText2SkinScreen::DrawImage(int x, int y, const cImage &Image, const tColor *ColorMask)
{
	if (mCanvas)
		BlendPixels(x, y, Image.Width(), Image.Height(), Image.Data(), NULL, ColorMask);
}

void cText2SkinScreen::DrawRectangle(int x1, int y1, int x2, int y2, tColor Color)
{
	if (mCanvas) {
		mCanvas->DrawRectangle(cRect(x1 - mOrigin.X(), y1 - mOrigin.Y(), x2 - x1 + 1, y2 - y1 + 1), Color);
		return;
	}
	Touch(x1, y1, x2, y2);
	for (int i = 0; i < mNumRegions; ++i)
		mRegions[i]->DrawRectangle(x1, y1, x2, y2, Color);
}

void cText2SkinScreen::DrawText(int x, int y, const char *s, tColor ColorFg, tColor ColorBg,
                                const cFont *Font, int Width, int Height, int Alignment)
{
	if (mCanvas) {
		// the canvas is layer 0, text with a transparent background is blended
		mCanvas->DrawText(cPoint(x - mOrigin.X(), y - mOrigin.Y()), s, ColorFg, ColorBg, Font, Width, Height, Alignment);
		return;
	}
	Touch(x, y, x + (Width ? Width : Font->Width(s)) - 1, y + (Height ? Height : Font->Height()) - 1);
	for (int i = 0; i < mNumRegions; ++i)
		mRegions[i]->DrawText(x, y, s, ColorFg, ColorBg, Font, Width, Height, Alignment);
}

void cText2SkinScreen::DrawEllipse(int x1, int y1, int x2, int y2, tColor Color, int Quadrants)
{
	if (mCanvas) {
		mCanvas->DrawEllipse(cRect(x1 - mOrigin.X(), y1 - mOrigin.Y(), x2 - x1 + 1, y2 - y1 + 1), Color, Quadrants);
		return;
	}
	Touch(x1, y1, x2, y2);
	for (int i = 0; i < mNumRegions; ++i)
		mRegions[i]->DrawEllipse(x1, y1, x2, y2, Color, Quadrants);
}

void cText2SkinScreen::DrawSlope(int x1, int y1, int x2, int y2, tColor Color, int Type)
{
	if (mCanvas) {
		mCanvas->DrawSlope(cRect(x1 - mOrigin.X(), y1 - mOrigin.Y(), x2 - x1 + 1, y2 - y1 + 1), Color, Type);
		return;
	}
	Touch(x1, y1, x2, y2);
	for (int i = 0; i < mNumRegions; ++i)
		mRegions[i]->DrawSlope(x1, y1, x2, y2, Color, Type);
}

void cText2SkinScreen::Flush(void)
{
	if (mCanvas) {
		// hand only the changed rectangle to the OSD
		{
			LOCK_PIXMAPS;
			cRect r = mCanvas->DirtyDrawPort();
			if (!r.IsEmpty()) {
				cImage part(r.Size());
				int stride = mCanvas->DrawPort().Width();
				const tColor *src = mCanvas->Pixels() + r.Top() * stride + r.Left();
				tColor *dst = (tColor *)part.Data();
				for (int iy = 0; iy < r.Height(); ++iy, src += stride, dst += r.Width())
					memcpy(dst, src, r.Width() * sizeof(tColor));
				mOsd->DrawImage(r.Point(), part);
				mCanvas->SetClean();
			}
		}
		mOsd->Flush();
		return;
	}

	for (int i = 0; i < mNumRegions; ++i) {
		// windows without drawing since the last flush need no copy
		if (!mTouched[i])
			continue;
		if (mOffScreen)
			mScreen->DrawBitmap(mRegions[i]->X0(), mRegions[i]->Y0(), *mRegions[i]);
		else
			mOsd->DrawBitmap(mRegions[i]->X0(), mRegions[i]->Y0(), *mRegions[i]);
		mTouched[i] = false;
	}
	if (!mOffScreen)
		mOsd->Flush();
}

void cText2SkinScreen::DrawBitmapOverlay(cBitmap &Dest, int x, int y, cBitmap &Bitmap,
                                         const tColor *ColorMask)
{
	const tIndex *bitmap = Dest.Data(0, 0);
	if (bitmap && Bitmap.Data(0, 0) && Dest.Intersects(x, y, x + Bitmap.Width() - 1, y + Bitmap.Height() - 1)) {
		if (Dest.Covers(x, y, x + Bitmap.Width() - 1, y + Bitmap.Height() - 1))
			Dest.Reset();
		x -= Dest.X0();
		y -= Dest.Y0();
		tIndex Indexes[MAXNUMCOLORS];
		Dest.Take(Bitmap, &Indexes);
		tIndex Mask, *indexMask = NULL;
		if (ColorMask != NULL) {
			Mask = Dest.Index(*ColorMask);
			indexMask = &Mask;
		}
		for (int ix = 0; ix < Bitmap.Width(); ix++) {
			for (int iy = 0; iy < Bitmap.Height(); iy++) {
				if (indexMask == NULL || *indexMask != Indexes[int(*Bitmap.Data(ix, iy))])
					Dest.SetIndex(x + ix, y + iy, Indexes[int(*Bitmap.Data(ix, iy))]);
			}
		}
	}
}
