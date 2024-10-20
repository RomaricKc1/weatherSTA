#include "btn.h"

#include <Arduino.h>

Btn::Btn(/* args */)
{
	this->pin = -1;
}

Btn::Btn(const uint8_t pin, uint8_t mode)
{
	this->pin = pin;

	if (mode == PU)
	{
		pinMode(this->pin, INPUT_PULLUP);
		this->btnPressed = 0;
	}
	else if (mode == PD)
	{
		pinMode(this->pin, INPUT_PULLDOWN);
		this->btnPressed = 1;
	}
	this->flag = NA;
}

Btn::~Btn()
{
}

int Btn::getBtn_state(void)
{
	return this->flag;
}

void Btn::clearBtn_state(void)
{
	this->flag = NA;
}

void Btn::tick(void)
{
	if (this->pin > 0)
	{
		tick(digitalRead(this->pin) == btnPressed);
	}
}

void Btn::newState(FSM_t new_state)
{
	this->prev_state = this->state;
	this->state = new_state;
}

void Btn::tick(uint8_t activeLevel)
{
	unsigned long now = millis(); // current (relative) time in msecs.
	unsigned long waitTime = (now - this->start_T);

	// Implementation of the state machine
	switch (state)
	{
	case Btn::INIT:
		// waiting for level to become active.
		if (activeLevel == 1)
		{
			newState(Btn::DOWN);
			start_T = now;
			n_click = 0;
		} // if
		break;

	case Btn::DOWN:
		// waiting for level to become inactive.
		if ((activeLevel == 0) && (waitTime < debT))
		{
			// bouncing.
			newState(prev_state);
		}
		else if (activeLevel == 0)
		{
			newState(Btn::UP);
			start_T = now;
		}
		else
		{
			if ((activeLevel == 1) && (waitTime > pressT))
			{
				newState(Btn::PRESS);
			}
		}
		break;

	case Btn::UP:
		// btn not pushed
		if ((activeLevel == 1) && (waitTime < debT))
		{
			// bouncing.
			newState(prev_state);
		}
		else
		{
			if (waitTime >= debT)
			{
				// count as a short button down
				n_click++;
				newState(Btn::COUNT);
			}
		}
		break;

	case Btn::COUNT:
		// debounce time is over, count clicks
		if (activeLevel == 1)
		{
			// button is down again
			newState(Btn::DOWN);
			start_T = now;
		}
		else if ((waitTime > clickT) || (n_click == max_click))
		{
			// now we know how many clicks have been made.

			if (n_click == 1)
			{
				// this was 1 click only.
				this->flag = BTN_CLICK;
				if (click_foo)
					click_foo();
			}
			else if (n_click == 2)
			{
				// this was a 2 click sequence.
				this->flag = BTN_X2_CLICK;
				if (x2_click_foo)
					x2_click_foo();
			}
			else
			{
				// this was a multi click sequence.
				this->flag = BTN_X3_CLICK;
				if (x_click_foo)
					x_click_foo();
			}
			reset();
		}
		break;

	case Btn::PRESS:
		// waiting for pin being release after long press.
		if (activeLevel == 0)
		{
			newState(Btn::RELEASED);
			start_T = now;
		}
		else
		{
			// still the button is pressed
			;
		}
		break;

	case Btn::RELEASED:
		// button was released.
		if ((activeLevel == 1) && (waitTime < debT))
		{
			// bouncing.
			newState(prev_state); // go back
		}
		else
		{
			if (waitTime >= debT)
			{
				this->flag = BTN_PRESS;
				if (press_foo)
					press_foo();
				reset();
			}
		}
		break;

	default:
		// unknownreset state machine
		newState(Btn::INIT);
		break;
	}
}

void Btn::reset(void)
{
	this->state = INIT;
	this->prev_state = INIT;
	this->n_click = 0;
	this->start_T = 0;
	// this->flag       = NA;
}
