export const dirname = (_path: string) => '/';

export const join = (...segments: string[]) =>
  segments.filter((segment) => segment && segment.length > 0).join('/');

export const resolve = (...segments: string[]) => join(...segments);

export default {
  dirname,
  join,
  resolve,
};
