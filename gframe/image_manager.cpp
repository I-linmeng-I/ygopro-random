#include "image_manager.h"
#include "game.h"
#include <thread>
#ifdef _OPENMP
#include <omp.h>
#endif

namespace ygo {

ImageManager imageManager;

bool ImageManager::Initial() {
	tCover[0] = nullptr;
	tCover[1] = nullptr;
	tCover[2] = GetTextureFromFile("textures/cover.jpg", CARD_IMG_WIDTH, CARD_IMG_HEIGHT);
	tCover[3] = GetTextureFromFile("textures/cover2.jpg", CARD_IMG_WIDTH, CARD_IMG_HEIGHT);
	if(!tCover[3])
		tCover[3] = tCover[2];
	tUnknown = nullptr;
	tUnknownFit = nullptr;
	tUnknownThumb = nullptr;
	tBigPicture = nullptr;
	tLoading = nullptr;
	tThumbLoadingThreadRunning = false;
	tAct = driver->getTexture("textures/act.png");
	tAttack = driver->getTexture("textures/attack.png");
	tChain = driver->getTexture("textures/chain.png");
	tNegated = driver->getTexture("textures/negated.png");
	tNumber = driver->getTexture("textures/number.png");
	tLPBar = driver->getTexture("textures/lp.png");
	tLPFrame = driver->getTexture("textures/lpf.png");
	tMask = driver->getTexture("textures/mask.png");
	tEquip = driver->getTexture("textures/equip.png");
	tTarget = driver->getTexture("textures/target.png");
	tChainTarget = driver->getTexture("textures/chaintarget.png");
	tLim = driver->getTexture("textures/lim.png");
	tOT = driver->getTexture("textures/ot.png");
	tHand[0] = driver->getTexture("textures/f1.jpg");
	tHand[1] = driver->getTexture("textures/f2.jpg");
	tHand[2] = driver->getTexture("textures/f3.jpg");
	tBackGround = nullptr;
	tBackGround_menu = nullptr;
	tBackGround_deck = nullptr;
	tField[0] = driver->getTexture("textures/field2.png");
	tFieldTransparent[0] = driver->getTexture("textures/field-transparent2.png");
	tField[1] = driver->getTexture("textures/field3.png");
	tFieldTransparent[1] = driver->getTexture("textures/field-transparent3.png");
	ResizeTexture();
	return true;
}
void ImageManager::SetDevice(irr::IrrlichtDevice* dev) {
	device = dev;
	driver = dev->getVideoDriver();
}
void ImageManager::ClearTexture() {
	for(auto tit = tMap[0].begin(); tit != tMap[0].end(); ++tit) {
		if(tit->second)
			driver->removeTexture(tit->second);
	}
	for(auto tit = tMap[1].begin(); tit != tMap[1].end(); ++tit) {
		if(tit->second)
			driver->removeTexture(tit->second);
	}
	for(auto tit = tThumb.begin(); tit != tThumb.end(); ++tit) {
		if(tit->second && tit->second != tLoading)
			driver->removeTexture(tit->second);
	}
	if(tBigPicture != nullptr) {
		driver->removeTexture(tBigPicture);
		tBigPicture = nullptr;
	}
	tMap[0].clear();
	tMap[1].clear();
	tThumb.clear();
	tThumbLoadingMutex.lock();
	tThumbLoading.clear();
	while(!tThumbLoadingCodes.empty())
		tThumbLoadingCodes.pop();
	tThumbLoadingThreadRunning = false;
	tThumbLoadingMutex.unlock();
	tFields.clear();
}
void ImageManager::RemoveTexture(int code) {
	auto tit = tMap[0].find(code);
	if(tit != tMap[0].end()) {
		if(tit->second)
			driver->removeTexture(tit->second);
		tMap[0].erase(tit);
	}
	tit = tMap[1].find(code);
	if(tit != tMap[1].end()) {
		if(tit->second)
			driver->removeTexture(tit->second);
		tMap[1].erase(tit);
	}
}
void ImageManager::ResizeTexture() {
	irr::s32 imgWidth = CARD_IMG_WIDTH * mainGame->xScale;
	irr::s32 imgHeight = CARD_IMG_HEIGHT * mainGame->yScale;
	irr::s32 imgWidthThumb = CARD_THUMB_WIDTH * mainGame->xScale;
	irr::s32 imgHeightThumb = CARD_THUMB_HEIGHT * mainGame->yScale;
	float mul = (mainGame->xScale > mainGame->yScale) ? mainGame->yScale : mainGame->xScale;
	irr::s32 imgWidthFit = CARD_IMG_WIDTH * mul;
	irr::s32 imgHeightFit = CARD_IMG_HEIGHT * mul;
	irr::s32 bgWidth = 1024 * mainGame->xScale;
	irr::s32 bgHeight = 640 * mainGame->yScale;
	driver->removeTexture(tCover[0]);
	driver->removeTexture(tCover[1]);
	tCover[0] = GetTextureFromFile("textures/cover.jpg", imgWidth, imgHeight);
	tCover[1] = GetTextureFromFile("textures/cover2.jpg", imgWidth, imgHeight);
	if(!tCover[1])
		tCover[1] = tCover[0];
	driver->removeTexture(tUnknown);
	driver->removeTexture(tUnknownFit);
	driver->removeTexture(tUnknownThumb);
	driver->removeTexture(tLoading);
	tLoading = GetTextureFromFile("textures/cover.jpg", imgWidthThumb, imgHeightThumb);
	tUnknown = GetTextureFromFile("textures/unknown.jpg", CARD_IMG_WIDTH, CARD_IMG_HEIGHT);
	tUnknownFit = GetTextureFromFile("textures/unknown.jpg", imgWidthFit, imgHeightFit);
	tUnknownThumb = GetTextureFromFile("textures/unknown.jpg", imgWidthThumb, imgHeightThumb);
	driver->removeTexture(tBackGround);
	tBackGround = GetTextureFromFile("textures/bg.jpg", bgWidth, bgHeight);
	driver->removeTexture(tBackGround_menu);
	tBackGround_menu = GetTextureFromFile("textures/bg_menu.jpg", bgWidth, bgHeight);
	if(!tBackGround_menu)
		tBackGround_menu = tBackGround;
	driver->removeTexture(tBackGround_deck);
	tBackGround_deck = GetTextureFromFile("textures/bg_deck.jpg", bgWidth, bgHeight);
	if(!tBackGround_deck)
		tBackGround_deck = tBackGround;
}
// function by Warr1024, from https://github.com/minetest/minetest/issues/2419 , modified
void imageScaleNNAA(irr::video::IImage *src, irr::video::IImage *dest) {
	const auto& srcDim = src->getDimension();
	const auto& destDim = dest->getDimension();

	// Cache scale ratios.
	const double rx = (double)srcDim.Width / destDim.Width;
	const double ry = (double)srcDim.Height / destDim.Height;

#pragma omp parallel if(mainGame->gameConf.use_image_scale_multi_thread)
{
	double sx, sy, minsx, maxsx, minsy, maxsy, area, ra, ga, ba, aa, pw, ph, pa;
	irr::video::SColor pxl, npxl;

	// Walk each destination image pixel.
#pragma omp for schedule(dynamic)
	for(irr::s32 dy = 0; dy < (irr::s32)destDim.Height; dy++) {
		for(irr::s32 dx = 0; dx < (irr::s32)destDim.Width; dx++) {
			// Calculate floating-point source rectangle bounds.
			minsx = dx * rx;
			maxsx = minsx + rx;
			minsy = dy * ry;
			maxsy = minsy + ry;

			// Total area, and integral of r, g, b values over that area,
			// initialized to zero, to be summed up in next loops.
			area = 0;
			ra = 0;
			ga = 0;
			ba = 0;
			aa = 0;

			// Loop over the integral pixel positions described by those bounds.
			for(sy = floor(minsy); sy < maxsy; sy++) {
				for(sx = floor(minsx); sx < maxsx; sx++) {
					// Calculate width, height, then area of dest pixel
					// that's covered by this source pixel.
					pw = 1;
					if(minsx > sx)
						pw += sx - minsx;
					if(maxsx < (sx + 1))
						pw += maxsx - sx - 1;
					ph = 1;
					if(minsy > sy)
						ph += sy - minsy;
					if(maxsy < (sy + 1))
						ph += maxsy - sy - 1;
					pa = pw * ph;

					// Get source pixel and add it to totals, weighted
					// by covered area and alpha.
					pxl = src->getPixel((irr::u32)sx, (irr::u32)sy);
					area += pa;
					ra += pa * pxl.getRed();
					ga += pa * pxl.getGreen();
					ba += pa * pxl.getBlue();
					aa += pa * pxl.getAlpha();
				}
			}
			// Set the destination image pixel to the average color.
			if(area > 0) {
				npxl.set((irr::u32)(aa / area + 0.5),
						 (irr::u32)(ra / area + 0.5),
						 (irr::u32)(ga / area + 0.5),
						 (irr::u32)(ba / area + 0.5));
			} else {
				npxl.set(0);
			}
			dest->setPixel(dx, dy, npxl);
		}
	}
} // end of parallel region
}
irr::video::ITexture* ImageManager::GetTextureFromFile(const char* file, irr::s32 width, irr::s32 height) {
	if(mainGame->gameConf.use_image_scale) {
		irr::video::ITexture* texture;
		irr::video::IImage* srcimg = driver->createImageFromFile(file);
		if(srcimg == nullptr)
			return nullptr;
		if(srcimg->getDimension() == irr::core::dimension2d<irr::u32>(width, height)) {
			texture = driver->addTexture(file, srcimg);
		} else {
			irr::video::IImage *destimg = driver->createImage(srcimg->getColorFormat(), irr::core::dimension2d<irr::u32>(width, height));
			imageScaleNNAA(srcimg, destimg);
			texture = driver->addTexture(file, destimg);
			destimg->drop();
		}
		srcimg->drop();
		return texture;
	} else {
		return driver->getTexture(file);
	}
}
irr::video::ITexture* ImageManager::GetTexture(int code, bool fit) {
	if(code == 0)
		return fit ? tUnknownFit : tUnknown;
	int width = CARD_IMG_WIDTH;
	int height = CARD_IMG_HEIGHT;
	if(fit) {
		float mul = mainGame->xScale;
		if(mainGame->xScale > mainGame->yScale)
			mul = mainGame->yScale;
		width = width * mul;
		height = height * mul;
	}
	auto tit = tMap[fit ? 1 : 0].find(code);
	if(tit == tMap[fit ? 1 : 0].end()) {
		char file[256];
		std::snprintf(file, sizeof file, "expansions/pics/%d.jpg", code);
		irr::video::ITexture* img = GetTextureFromFile(file, width, height);
		if(img == nullptr) {
			std::snprintf(file, sizeof file, "pics/%d.jpg", code);
			img = GetTextureFromFile(file, width, height);
		}
		if(img == nullptr && !mainGame->gameConf.use_image_scale) {
			tMap[fit ? 1 : 0][code] = nullptr;
			return GetTextureThumb(code);
		}
		tMap[fit ? 1 : 0][code] = img;
		return (img == nullptr) ? (fit ? tUnknownFit : tUnknown) : img;
	}
	if(tit->second)
		return tit->second;
	else
		return mainGame->gameConf.use_image_scale ? (fit ? tUnknownFit : tUnknown) : GetTextureThumb(code);
}
irr::video::ITexture* ImageManager::GetBigPicture(int code, float zoom) {
	if(code == 0)
		return tUnknown;
	if(tBigPicture != nullptr) {
		driver->removeTexture(tBigPicture);
		tBigPicture = nullptr;
	}
	irr::video::ITexture* texture;
	char file[256];
	std::snprintf(file, sizeof file, "expansions/pics/%d.jpg", code);
	irr::video::IImage* srcimg = driver->createImageFromFile(file);
	if(srcimg == nullptr) {
		std::snprintf(file, sizeof file, "pics/%d.jpg", code);
		srcimg = driver->createImageFromFile(file);
	}
	if(srcimg == nullptr) {
		return tUnknown;
	}
	if(zoom == 1) {
		texture = driver->addTexture(file, srcimg);
	} else {
		auto origsize = srcimg->getDimension();
		irr::video::IImage* destimg = driver->createImage(srcimg->getColorFormat(), irr::core::dimension2d<irr::u32>(origsize.Width * zoom, origsize.Height * zoom));
		imageScaleNNAA(srcimg, destimg);
		texture = driver->addTexture(file, destimg);
		destimg->drop();
	}
	srcimg->drop();
	tBigPicture = texture;
	return texture;
}
int ImageManager::LoadThumbThread() {
	while(true) {
		imageManager.tThumbLoadingMutex.lock();
		int code = imageManager.tThumbLoadingCodes.front();
		imageManager.tThumbLoadingCodes.pop();
		imageManager.tThumbLoadingMutex.unlock();
		char file[256];
		std::snprintf(file, sizeof file, "expansions/pics/thumbnail/%d.jpg", code);
		irr::video::IImage* img = imageManager.driver->createImageFromFile(file);
		if(img == nullptr) {
			std::snprintf(file, sizeof file, "pics/thumbnail/%d.jpg", code);
			img = imageManager.driver->createImageFromFile(file);
		}
		if(img == nullptr && mainGame->gameConf.use_image_scale) {
			std::snprintf(file, sizeof file, "expansions/pics/%d.jpg", code);
			img = imageManager.driver->createImageFromFile(file);
		}
		if(img == nullptr && mainGame->gameConf.use_image_scale) {
			std::snprintf(file, sizeof file, "pics/%d.jpg", code);
			img = imageManager.driver->createImageFromFile(file);
		}
		if(img != nullptr) {
			int width = CARD_THUMB_WIDTH * mainGame->xScale;
			int height = CARD_THUMB_HEIGHT * mainGame->yScale;
			if(img->getDimension() == irr::core::dimension2d<irr::u32>(width, height)) {
				img->grab();
				imageManager.tThumbLoadingMutex.lock();
				if(imageManager.tThumbLoadingThreadRunning)
					imageManager.tThumbLoading[code] = img;
				imageManager.tThumbLoadingMutex.unlock();
			} else {
				irr::video::IImage *destimg = imageManager.driver->createImage(img->getColorFormat(), irr::core::dimension2d<irr::u32>(width, height));
				imageScaleNNAA(img, destimg);
				img->drop();
				destimg->grab();
				imageManager.tThumbLoadingMutex.lock();
				if(imageManager.tThumbLoadingThreadRunning)
					imageManager.tThumbLoading[code] = destimg;
				imageManager.tThumbLoadingMutex.unlock();
			}
		} else {
			imageManager.tThumbLoadingMutex.lock();
			if(imageManager.tThumbLoadingThreadRunning)
				imageManager.tThumbLoading[code] = nullptr;
			imageManager.tThumbLoadingMutex.unlock();
		}
		imageManager.tThumbLoadingMutex.lock();
		imageManager.tThumbLoadingThreadRunning = !imageManager.tThumbLoadingCodes.empty();
		if(!imageManager.tThumbLoadingThreadRunning)
			break;
		imageManager.tThumbLoadingMutex.unlock();
	}
	imageManager.tThumbLoadingMutex.unlock();
	return 0;
}
irr::video::ITexture* ImageManager::GetTextureThumb(int code) {
	if(code == 0)
		return tUnknownThumb;
	auto tit = tThumb.find(code);
	if(tit == tThumb.end() && !mainGame->gameConf.use_image_load_background_thread) {
		char file[256];
		std::snprintf(file, sizeof file, "expansions/pics/thumbnail/%d.jpg", code);
		int width = CARD_THUMB_WIDTH * mainGame->xScale;
		int height = CARD_THUMB_HEIGHT * mainGame->yScale;
		irr::video::ITexture* img = GetTextureFromFile(file, width, height);
		if(img == NULL) {
			std::snprintf(file, sizeof file, "pics/thumbnail/%d.jpg", code);
			img = GetTextureFromFile(file, width, height);
		}
		if(img == NULL && mainGame->gameConf.use_image_scale) {
			std::snprintf(file, sizeof file, "expansions/pics/%d.jpg", code);
			img = GetTextureFromFile(file, width, height);
			if(img == NULL) {
				std::snprintf(file, sizeof file, "pics/%d.jpg", code);
				img = GetTextureFromFile(file, width, height);
			}
		}
		tThumb[code] = img;
		return (img == NULL) ? tUnknownThumb : img;
	}
	if(tit == tThumb.end() || tit->second == tLoading) {
		imageManager.tThumbLoadingMutex.lock();
		auto lit = tThumbLoading.find(code);
		if(lit != tThumbLoading.end()) {
			if(lit->second != nullptr) {
				char file[256];
				std::snprintf(file, sizeof file, "pics/thumbnail/%d.jpg", code);
				irr::video::ITexture* texture = driver->addTexture(file, lit->second); // textures must be added in the main thread due to OpenGL
				lit->second->drop();
				tThumb[code] = texture;
			} else {
				tThumb[code] = nullptr;
			}
			tThumbLoading.erase(lit);
		}
		imageManager.tThumbLoadingMutex.unlock();
		tit = tThumb.find(code);
	}
	if(tit == tThumb.end()) {
		tThumb[code] = tLoading;
		imageManager.tThumbLoadingMutex.lock();
		tThumbLoadingCodes.push(code);
		if(!tThumbLoadingThreadRunning) {
			tThumbLoadingThreadRunning = true;
			std::thread(LoadThumbThread).detach();
		}
		imageManager.tThumbLoadingMutex.unlock();
		return tLoading;
	}
	if(tit->second)
		return tit->second;
	else
		return tUnknownThumb;
}
irr::video::ITexture* ImageManager::GetTextureField(int code) {
	if(code == 0)
		return nullptr;
	auto tit = tFields.find(code);
	if(tit == tFields.end()) {
		char file[256];
		std::snprintf(file, sizeof file, "expansions/pics/field/%d.png", code);
		irr::video::ITexture* img = GetTextureFromFile(file, 512 * mainGame->xScale, 512 * mainGame->yScale);
		if(img == nullptr) {
			std::snprintf(file, sizeof file, "expansions/pics/field/%d.jpg", code);
			img = GetTextureFromFile(file, 512 * mainGame->xScale, 512 * mainGame->yScale);
		}
		if(img == nullptr) {
			std::snprintf(file, sizeof file, "pics/field/%d.png", code);
			img = GetTextureFromFile(file, 512 * mainGame->xScale, 512 * mainGame->yScale);
		}
		if(img == nullptr) {
			std::snprintf(file, sizeof file, "pics/field/%d.jpg", code);
			img = GetTextureFromFile(file, 512 * mainGame->xScale, 512 * mainGame->yScale);
			if(img == nullptr) {
				tFields[code] = nullptr;
				return nullptr;
			} else {
				tFields[code] = img;
				return img;
			}
		} else {
			tFields[code] = img;
			return img;
		}
	}
	if(tit->second)
		return tit->second;
	else
		return nullptr;
}
}
