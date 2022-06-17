Show debug info to output window: 

```c++
std::wostringstream woss;
woss << mLightRotationAngle;
OutputDebugString(woss.str().c_str());
OutputDebugString(L"\n");
```

