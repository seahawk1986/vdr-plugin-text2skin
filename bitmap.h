//								-*- c++ -*-

#ifndef VDR_TEXT2SKIN_BITMAP_H
#define VDR_TEXT2SKIN_BITMAP_H

#include "common.h"
#include "cache.h"
#include <vdr/osd.h>

struct tBitmapSpec {
	std::string Filename;
	int         Alpha;
	int         Width;
	int         Height;
	int         Colors;
	bool        TrueColor;

	tBitmapSpec(const std::string &filename, int alpha, int width, int height, int colors,
	            bool trueColor = false):
		Filename(filename), Alpha(alpha), Width(width), Height(height), Colors(colors),
		TrueColor(trueColor) {}

	bool operator<(const tBitmapSpec &Src) const;
	bool operator==(const tBitmapSpec &Src) const;
};

inline bool tBitmapSpec::operator<(const tBitmapSpec &Src) const
{
	if (Filename == Src.Filename) {
		if (Alpha == Src.Alpha) {
			if (Width == Src.Width) {
				if (Height == Src.Height) {
					if (Colors == Src.Colors)
						return TrueColor < Src.TrueColor;
					return Colors < Src.Colors;
				}
				return Height < Src.Height;
			}
			return Width < Src.Width;
		}
		return Alpha < Src.Alpha;
	}
	return Filename < Src.Filename;
}

inline bool tBitmapSpec::operator==(const tBitmapSpec &Src) const
{
	return Filename == Src.Filename
	    && Alpha == Src.Alpha
		&& Width == Src.Width
		&& Height == Src.Height
		&& Colors == Src.Colors
		&& TrueColor == Src.TrueColor;
}

class cText2SkinBitmap;

class cBitmapCache: public cxCache<tBitmapSpec,cText2SkinBitmap*> {
protected:
	virtual void DeleteObject(const tBitmapSpec &Key, cText2SkinBitmap *&Data);
	virtual void ResetObject(cText2SkinBitmap *&Data);

public:
	cBitmapCache(uint MaxItems): cxCache<tBitmapSpec,cText2SkinBitmap*>(MaxItems) {}
	virtual ~cBitmapCache() { Flush(); }
};

class cText2SkinBitmap {
private:
	static cBitmapCache   mCache;

	std::vector<cBitmap*> mBitmaps; // palette frames (XPM, or images for a 256 colour OSD)
	std::vector<cImage*>  mImages;  // ARGB frames for a true colour OSD
	int                   mCurrent;
	time_t                mDelay;
	time_t                mLastGet;

	// disallow direct construction
	cText2SkinBitmap(void);

	bool LoadXpm(const char *Filename);
	bool LoadNonXpm(const char *Filename, int height, int width, int colors, bool Quiet,
	                bool TrueColor);
	void NextFrame(uint &UpdateIn, uint Now);

public:
	// TrueColor loads images as ARGB without colour reduction, XPM stays a palette bitmap
	static cText2SkinBitmap *Load(const std::string &Filename, int Alpha = 0, int height = 0,
	                              int width = 0, int colors = 0, bool Quiet = false,
	                              bool TrueColor = false);
	static bool Available(const std::string &Filename, int Alpha = 0, int height = 0,
	                      int width = 0, int colors = 0);
	static void ResetCache(void) { mCache.Reset(); }
	static void FlushCache(void) { mCache.Flush(); }
	static void Init(void);

	virtual ~cText2SkinBitmap();

	void Reset(void) { mCurrent = 0; mLastGet = 0; }
	bool IsImage(void) const { return !mImages.empty(); }
	cBitmap &Get(uint &UpdateIn, uint Now);
	const cImage &GetImage(uint &UpdateIn, uint Now);
	void SetColor(int Index, tColor Color);
	void SetAlpha(int Alpha);
};

inline void cText2SkinBitmap::SetColor(int Index, tColor Color) {
	if (!mBitmaps.empty())
		mBitmaps[mCurrent]->SetColor(Index, Color);
}

#endif // VDR_TEXT2SKIN_BITMAP_H
