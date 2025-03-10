#!/usr/bin/env node

import startCodebase from './scripts/codebase.js';
import startServer from './scripts/server.js';
import startClient from './scripts/client.js';

import fs from 'fs';
import YAML from 'yaml';

import { join, dirname } from 'path';
import { program } from 'commander';

async function start(testcase, options) {
  const basePath = join(dirname(new URL(import.meta.url).pathname), testcase);
  const config = YAML.parse(fs.readFileSync(join(basePath, 'plan.yaml'), 'utf8'));

  if (config.env) {
    const variables = {};
    for (const k in config.env) {
      let v = process.env[k] || config.env[k];
      variables[k] = v;
    }

    function replace(obj) {
      for (const k in obj) {
        const v = obj[k];
        switch (typeof v) {
          case 'string':
            obj[k] = v.replace(
              /\${\w+}/g,
              s => {
                s = s.substring(2, s.length - 1);
                const v = variables[s];
                if (!v) throw new Error(`variable '${s}' not found in env`);
                return v;
              }
            );
            break;
          case 'object':
            replace(v);
            break;
        }
      }
    }

    if (config.server) replace(config.server);
    if (config.client) replace(config.client);
  }

  let {
    pipy,
    client,
    server,
  } = options;

  if (!pipy && !client && !server) {
    pipy = true;
    client = true;
    server = true;
  }

  let repo, worker;

  if (pipy) ({ repo, worker } = await startCodebase(testcase, basePath));
  if (server) await startServer(config.server, basePath);
  if (client) await startClient(config.client, basePath, options.target);

  if (client && config.client?.duration) {
    setTimeout(
      () => {
        if (worker) worker.kill();
        if (repo) repo.kill();
        console.log('Done.');
        process.exit();
      },
      config.client.duration * 1000
    );
  }

  process.on('uncaughtException', err => {
    if (worker) worker.kill();
    if (repo) repo.kill();
    console.error('Uncaught exception:', err.message);
    console.error('  Stack trace:', err.stack);
    process.exit(-1);
  });
}

program
  .argument('<testcase>')
  .option('-p, --pipy', 'Run the Pipy codebase under testing')
  .option('-c, --client', 'Run the test client')
  .option('-s, --server', 'Run the mock server')
  .action((testcase, options) => start(testcase, options))
  .parse(process.argv)
