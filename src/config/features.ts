// Build-time switches. The weather widget can be left out of a build (HEARTH_WEATHER=0 npm run build): its code is
// then removed from the bundle, not just hidden.

declare const __WEATHER__: boolean | undefined;

export const WEATHER_ENABLED: boolean = typeof __WEATHER__ === 'undefined' ? true : __WEATHER__;
