export const readFileSync = () => {
  throw new Error('fs module is not available in the browser runtime');
};

export const existsSync = () => false;

export const statSync = () => ({
  isFile: () => false,
  isDirectory: () => false,
});

export default {
  readFileSync,
  existsSync,
  statSync,
};
