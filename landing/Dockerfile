FROM node:22-alpine
WORKDIR /app
COPY server.js ./
COPY public ./public
ENV PORT=10087
EXPOSE 10087
CMD ["node", "server.js"]
