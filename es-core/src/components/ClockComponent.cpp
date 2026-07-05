#include "components/ClockComponent.h"
#include "Settings.h"
#include <time.h>

ClockComponent::ClockComponent(Window* window) : TextComponent(window)
{
	mClockElapsed = 0;
}

void ClockComponent::update(int deltaTime)
{
	TextComponent::update(deltaTime);

	setVisible(Settings::getInstance()->getBool("DrawClock"));

	if (!isVisible())
		return;

	mClockElapsed -= deltaTime;
	if (mClockElapsed <= 0)
	{
		time_t     clockNow = time(0);
		struct tm  clockTstruct = *localtime(&clockNow);

		if (clockTstruct.tm_year > 100)
		{
			std::string clockBuf;
			if (Settings::getInstance()->getBool("ClockMode12"))
				clockBuf = Utils::Time::timeToString(clockNow, "%I:%M %p");
			else
				clockBuf = Utils::Time::timeToString(clockNow, "%H:%M");

			float oldWidth = mSize.x();

			// Recompute mSize to exactly fit the new text, avoiding "..." truncation
			mAutoCalcExtent.x() = 1;
			setText(clockBuf);

			float newWidth = mSize.x();

			// Keep the right edge fixed (clock is typically anchored to the right side)
			mPosition.x() -= (newWidth - oldWidth);
		}

		mClockElapsed = 1000; // next update in 1000ms
	}
}
