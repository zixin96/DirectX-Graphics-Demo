#ifndef GAMETIMER_H
#define GAMETIMER_H

/**
 * \brief This class uses the performance timer (counter) for accurate time measurements
 * e.g., it's used to measure the amount of time that elapses between frames of animation
 */
class GameTimer
{
public:
	GameTimer();

	float TotalTime() const; // in seconds
	float DeltaTime() const; // in seconds

	void Reset(); // Call before message loop.
	void Start(); // Call when unpaused.
	void Stop();  // Call when paused.
	void Tick();  // Call every frame.

private:
	double mSecondsPerCount;
	double mDeltaTime;

	__int64 mBaseTime;   // stores the time when the application started
	__int64 mPausedTime; // accumulates all the time that passes while we are paused 
	__int64 mStopTime;   // gives us the time when the timer is stopped (paused)
	__int64 mPrevTime;
	__int64 mCurrTime;

	bool mStopped;
};

#endif // GAMETIMER_H
