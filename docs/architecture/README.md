# LavaLite architecture presentations

- `lavalite-architecture.pdf`: batch scheduler architecture (mbd, sbd,
  scheduling, execution, job lifetime, and recovery).
- `lavalite-service-architecture.pdf`: companion presentation about
  persistent service jobs (maintained separately).

The batch presentation can be edited in `lavalite-architecture.pptx` or
regenerated from `lavalite-architecture.js` with Node.js and `pptxgenjs`:

```sh
node lavalite-architecture.js .
libreoffice --headless --convert-to pdf lavalite-architecture.pptx
```

The presentations describe the current architecture without embedding
release numbers. Refer to the administrator guide and release notes for
version-specific changes, configuration details, and known limitations.
