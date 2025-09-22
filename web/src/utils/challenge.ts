export const formatChallengeDecimal = (value: number): string => {
  const padded = value.toString().padStart(10, '0');
  return padded.replace(/(\d{2})(?=\d)/g, '$1 ');
};

export const formatChallengeHex = (value: number): string =>
  value.toString(16).padStart(8, '0').toUpperCase();
