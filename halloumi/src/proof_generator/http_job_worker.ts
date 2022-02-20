import { Server } from '../server';
import { Command, Protocol } from './http_job_protocol';
import { fetch } from '@aztec/barretenberg/iso_fetch';
import debug from 'debug';
import { randomBytes } from 'crypto';

export class InterruptableSleep {
  private interruptResolve = () => {};
  private interruptPromise = new Promise<void>(resolve => (this.interruptResolve = resolve));
  private timeouts: NodeJS.Timeout[] = [];

  public async sleep(ms: number) {
    let timeout!: NodeJS.Timeout;
    const promise = new Promise(resolve => (timeout = setTimeout(resolve, ms)));
    this.timeouts.push(timeout);
    await Promise.race([promise, this.interruptPromise]);
    clearTimeout(timeout);
    this.timeouts.splice(this.timeouts.indexOf(timeout), 1);
  }

  public interrupt() {
    this.interruptResolve();
    this.interruptPromise = new Promise(resolve => (this.interruptResolve = resolve));
  }
}

export class HttpJobWorker {
  private id = randomBytes(32);
  private running = true;
  private runningPromise!: Promise<void>;
  private log = debug(`http_job_worker:${this.id.slice(0, 4).toString('hex')}`);
  private interruptableSleep = new InterruptableSleep();
  private abortController!: AbortController;
  private pingTimeout!: NodeJS.Timeout;

  constructor(public readonly server: Server, private url = 'http://localhost:8082') {}

  public start() {
    this.log('start');
    this.runningPromise = this.runLoop();
  }

  async stop() {
    this.log('stop called');
    this.running = false;
    this.abortController.abort();
    this.interruptableSleep.interrupt();
    clearTimeout(this.pingTimeout);
    await this.runningPromise;
    this.log('stop complete');
  }

  private async runLoop() {
    while (this.running) {
      try {
        this.log('sending request for work');

        this.abortController = new AbortController();
        const resp = await fetch(`${this.url}/get-job`, {
          signal: this.abortController.signal,
        }).catch(() => undefined);
        if (!resp) {
          this.log('failed to get job');
          if (this.running) {
            await this.interruptableSleep.sleep(1000);
          }
          continue;
        }

        const work = Buffer.from(await resp.arrayBuffer());
        if (!work.length) {
          this.log('no work received');
          await this.interruptableSleep.sleep(1000);
          continue;
        }

        this.log('received job:', Protocol.logUnpack(work).id);
        const { id, cmd, data } = Protocol.unpack(work);

        this.pingTimeout = setTimeout(() => this.ping(id), 1000);

        const res = await this.process(cmd, data);
        const repBuf = Protocol.pack(id, cmd, res);

        await fetch(`${this.url}/job-complete`, { body: repBuf, method: 'POST' }).catch(() => undefined);
        clearTimeout(this.pingTimeout);
      } catch (err) {
        console.log(err);
        clearTimeout(this.pingTimeout);
        await this.interruptableSleep.sleep(1000);
      }
    }
    this.log('exiting function loop');
  }

  private async ping(jobId: Buffer) {
    this.log('ping...');
    try {
      await fetch(`${this.url}/ping?job-id=${jobId.toString('hex')}`);
    } finally {
      this.pingTimeout = setTimeout(() => this.ping(jobId), 1000);
    }
  }

  private async process(cmd: number, data: Buffer) {
    this.log(Command[cmd]);
    switch (cmd) {
      case Command.GET_JOIN_SPLIT_VK:
        return await this.server.getJoinSplitVerificationKey();
      case Command.GET_ACCOUNT_VK:
        return await this.server.getAccountVerificationKey();
      case Command.CREATE_PROOF:
        return await this.server.createProof(data);
    }
  }
}
