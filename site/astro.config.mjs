// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import starlightLinksValidator from 'starlight-links-validator';

// The site is published by the "Docs" workflow to GitHub Pages at
// https://tsln-lab.github.io/libear-max/ (hence the base path).
export default defineConfig({
  site: 'https://tsln-lab.github.io',
  base: '/libear-max',
  integrations: [
    starlight({
      title: 'libear-max',
      description:
        'Max externals for rendering ADM audio objects with libear, the EBU implementation of the ITU-R BS.2127 renderer.',
      social: [
        { icon: 'github', label: 'GitHub', href: 'https://github.com/tsln-lab/libear-max' },
      ],
      editLink: {
        baseUrl: 'https://github.com/tsln-lab/libear-max/edit/main/site/',
      },
      // fails the build on a link to a page or heading that does not exist
      plugins: [starlightLinksValidator()],
      sidebar: [
        { label: 'Overview', slug: '' },
        {
          label: 'Objects',
          items: [
            { label: 'ear.objects and ear.objects~', slug: 'objects/ear-objects' },
            { label: 'mc.ear.objects~ and mc.ear.direct~', slug: 'objects/mc-ear-objects' },
            { label: 'ear.direct', slug: 'objects/ear-direct' },
            { label: 'mc.ear.hoa~', slug: 'objects/mc-ear-hoa' },
            { label: 'ear.hoa', slug: 'objects/ear-hoa' },
            { label: 'mc.ear.encode~', slug: 'objects/mc-ear-encode' },
            { label: 'Mixing beds, objects and scenes', slug: 'objects/mixing' },
          ],
        },
        {
          label: 'ADM files',
          items: [
            { label: 'ear.adm and mc.ear.select~', slug: 'adm/ear-adm' },
            { label: 'Dolby Atmos masters', slug: 'adm/dolby-atmos' },
            { label: 'mc.ear.play~', slug: 'adm/mc-ear-play' },
            { label: 'mc.ear.record~', slug: 'adm/mc-ear-record' },
            { label: 'The ADM test corpus', slug: 'adm/test-corpus' },
          ],
        },
        {
          label: 'Project',
          items: [
            { label: 'Reference parity', slug: 'reference-parity' },
            { label: 'Building', slug: 'building' },
            { label: 'Releases', slug: 'releases' },
            { label: 'Signing and notarization', slug: 'signing' },
            { label: 'License', slug: 'license' },
          ],
        },
        {
          label: 'Development',
          items: [
            { label: 'Repository layout', slug: 'development/repository-layout' },
            { label: 'Reference pages', slug: 'development/reference-pages' },
            { label: 'Design notes', slug: 'development/design-notes' },
            { label: 'Roadmap', slug: 'development/roadmap' },
          ],
        },
      ],
    }),
  ],
});
