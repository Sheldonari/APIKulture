# APIkulture website

Static site for [https://www.apikulture.com](https://www.apikulture.com). Source for the app lives in the repository root; this folder is only the public web presence. Repository: [Sheldonari/APIKulture on Codeberg](https://codeberg.org/Sheldonari/APIKulture).

## Contents

| Path | Purpose |
|------|--------|
| `index.html` | Landing page |
| `css/style.css` | Styles |
| `assets/apikulture.svg` | Favicon and hero art (copy of `ui/icons/apikulture.svg` in the app tree) |
| `robots.txt` | Crawler hints (allows all, declares sitemap) |
| `sitemap.xml` | XML sitemap submitted to Google and Bing |
| `nginx-vhost.conf` | nginx virtual host — includes the apex→www redirect required for indexing |
| `.domains` | Lists `www.apikulture.com` for Codeberg Pages custom domain setup |

## Deployment (nginx)

The site is served by nginx. Copy the static files to the web root and install the vhost config:

```sh
# Copy site files
rsync -av --delete website/ /var/www/apikulture.com/

# Install vhost (adjust path as needed)
cp website/nginx-vhost.conf /etc/nginx/sites-available/apikulture.com
ln -sf /etc/nginx/sites-available/apikulture.com /etc/nginx/sites-enabled/apikulture.com
nginx -t && systemctl reload nginx
```

### Why the nginx config matters for indexing

`nginx-vhost.conf` contains a **critical SEO redirect**: `https://apikulture.com` (apex) must return a `301` to `https://www.apikulture.com`. Without it, both the apex and `www` domains return `HTTP 200` for identical content. Google and Bing see two duplicate pages with no definitive server-side canonical, so they decline to index either one. The `<link rel="canonical">` in HTML is a hint only — a `301` redirect is the authoritative signal.

Current redirect chain after applying the config:

```
http://apikulture.com      → 301 → https://www.apikulture.com
http://www.apikulture.com  → 301 → https://www.apikulture.com
https://apikulture.com     → 301 → https://www.apikulture.com  ← was missing
https://www.apikulture.com → 200  (canonical URL)
```

## Codeberg Pages (alternative hosting)

1. Enable **Codeberg Pages** for your repository and follow the current [Codeberg Pages documentation](https://docs.codeberg.org/codeberg-pages/).
2. Publish the **contents of this `website/` directory** as the site root (or configure your workflow to copy `website/*` into the published branch).
3. Links in `index.html` point to **https://codeberg.org/Sheldonari/APIKulture** (hero, footer, README links). Update them if the repo moves.
4. **Custom domain:** add DNS records as Codeberg documents. The `.domains` file here declares `www.apikulture.com`; you may need to copy it to the root of the published site depending on your Pages setup.
5. Note: Codeberg Pages does not support custom nginx vhost rules. Ensure the apex domain has a DNS-level redirect to `www` (many DNS providers offer a redirect record or CNAME flattening for this).

## Syncing the icon

When you change `ui/icons/apikulture.svg`, copy it to `website/assets/apikulture.svg` (or automate in CI) so the site and app stay aligned.

## Updating structured data for new releases

When a new version ships, update the `softwareVersion` field in the `application/ld+json` block in `index.html` and update the `downloadUrl` links to point to the new release assets.
