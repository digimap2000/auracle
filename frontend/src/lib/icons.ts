/**
 * Standard icon sizes used throughout the application.
 * Lucide icons accept a numeric `size` prop in pixels.
 * Always use these constants — never pass raw numbers.
 */
export const ICON_SIZE = {
  /** Status bar indicators, inline tab icons */
  xs: 12,
  /** Secondary UI — badges, form labels, tab triggers */
  sm: 14,
  /** Primary UI — navigation, buttons, toolbar */
  md: 16,
  /** Emphasis — section headers, larger buttons */
  lg: 20,
  /** Standalone — dialog icons, empty states */
  xl: 24,
  /** Hero — large decorative icons, preview panels */
  hero: 40,
} as const;
