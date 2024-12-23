export function lock(path: string): Promise<number>
export function unlock(fd: number): Promise<void>
export function isLocked(path: string): Promise<boolean>